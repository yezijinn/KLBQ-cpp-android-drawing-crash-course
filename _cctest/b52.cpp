#include <cstdio>
#include <cstdint>
#include <vector>

class FileReader {
public:
    explicit FileReader(const char *path) {   // explicit：禁止"隐式转换"式构造，见下方说明
        fp_ = fopen(path, "rb");
        if (!fp_) { valid_ = false; return; }
        fseek(fp_, 0, SEEK_END);
        size_ = ftell(fp_);
        fseek(fp_, 0, SEEK_SET);
        valid_ = true;
    }
    ~FileReader() { if (fp_) fclose(fp_); }

    FileReader(const FileReader&) = delete;              // 禁止拷贝
    FileReader& operator=(const FileReader&) = delete;

    bool valid() const { return valid_; }
    size_t size() const { return size_; }

    bool read_all(std::vector<uint8_t> &out) {
        if (!valid_) return false;
        out.resize(size_);
        return fread(out.data(), 1, size_, fp_) == size_;
    }

private:
    FILE *fp_ = nullptr;
    size_t size_ = 0;
    bool valid_ = false;
};

int main(void) {
    FileReader f("test.bin");
    if (!f.valid()) { printf("打不开\n"); return 1; }
    std::vector<uint8_t> data;
    f.read_all(data);
    printf("读到了 %zu 字节\n", data.size());
    return 0;                       // f 析构，自动 fclose
}