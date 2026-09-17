void print(int arr[], int n) {   // 等价于 void print(int *arr, int n)
    printf("%zu\n", sizeof(arr));  // 输出 8（指针大小），不是数组大小！
}

int main(void) {
    int a[10];
    printf("%zu\n", sizeof(a));   // 输出 40
    print(a, 10);
}