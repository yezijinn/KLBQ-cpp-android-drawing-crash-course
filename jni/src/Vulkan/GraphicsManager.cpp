//
// Created by ITEK on 2024/2/3.
//

#include "GraphicsManager.h"
#include "VulkanGraphics.h"

std::unique_ptr<AndroidImgui> GraphicsManager::getGraphicsInterface() {
    return std::make_unique<VulkanGraphics>();
}
