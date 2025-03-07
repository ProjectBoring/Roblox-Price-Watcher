#include "ImageLoading.h"
#include "WebHandling.h" // Assuming this is where WebHandler is defined
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <nlohmann/json.hpp> // Add this for JSON parsing
#include <iostream>

ImageLoading::ImageLoading(ID3D11Device* device) : d3d_device(device) {
    // No additional initialization needed; device is stored for texture creation
}

ImageLoading::~ImageLoading() {
    // No cleanup needed here; textures are managed externally
}

ID3D11ShaderResourceView* ImageLoading::LoadTextureFromURL(const std::string& url) {
    WebHandler web;

    // Step 1: Fetch the JSON response from the Roblox API
    std::string json_data = web.get(url);
    if (json_data.empty()) {
        std::cerr << "Failed to fetch JSON from URL: " << url << std::endl;
        return nullptr;
    }

    // Step 2: Parse the JSON to extract the image URL
    try {
        nlohmann::json json = nlohmann::json::parse(json_data);
        if (!json.contains("data") || json["data"].empty()) {
            std::cerr << "Invalid JSON structure: no 'data' field or empty" << std::endl;
            return nullptr;
        }

        std::string image_url = json["data"][0]["imageUrl"].get<std::string>();
        if (image_url.empty()) {
            std::cerr << "No image URL found in JSON" << std::endl;
            return nullptr;
        }

        // Step 3: Fetch the actual image data from the extracted URL
        std::string image_data = web.get(image_url);
        if (image_data.empty()) {
            std::cerr << "Failed to fetch image data from: " << image_url << std::endl;
            return nullptr;
        }

        // Step 4: Load the image into memory using stb_image
        int width, height, channels;
        unsigned char* image = stbi_load_from_memory(
            reinterpret_cast<const unsigned char*>(image_data.data()),
            static_cast<int>(image_data.size()),
            &width, &height, &channels, 4 // Force RGBA
        );
        if (!image) {
            std::cerr << "Failed to load image from memory: " << image_url << std::endl;
            return nullptr;
        }

        // Step 5: Create the DirectX 11 texture (unchanged from your original code)
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA subResource;
        subResource.pSysMem = image;
        subResource.SysMemPitch = width * 4; // 4 bytes per pixel (RGBA)
        subResource.SysMemSlicePitch = 0;

        ID3D11Texture2D* texture = nullptr;
        HRESULT hr = d3d_device->CreateTexture2D(&desc, &subResource, &texture);
        stbi_image_free(image); // Free the image data after texture creation
        if (FAILED(hr)) {
            std::cerr << "Failed to create texture: " << image_url << std::endl;
            return nullptr;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        ZeroMemory(&srvDesc, sizeof(srvDesc));
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        ID3D11ShaderResourceView* srv = nullptr;
        hr = d3d_device->CreateShaderResourceView(texture, &srvDesc, &srv);
        texture->Release(); // Release the texture after creating the SRV
        if (FAILED(hr)) {
            std::cerr << "Failed to create shader resource view: " << image_url << std::endl;
            return nullptr;
        }

        return srv;
    }
    catch (const std::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return nullptr;
    }
}
