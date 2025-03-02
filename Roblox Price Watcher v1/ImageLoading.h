#ifndef IMAGE_LOADING_H
#define IMAGE_LOADING_H

#include <string>
#include <d3d11.h>
#include "WebHandling.h" // Assuming this defines WebHandler

class ImageLoading {
public:
    ImageLoading(ID3D11Device* device);
    ~ImageLoading();

    // Load a texture from a URL using WebHandler
    ID3D11ShaderResourceView* LoadTextureFromURL(const std::string& url);

private:
    ID3D11Device* d3d_device; // Pointer to the DirectX11 device
};

#endif // IMAGE_LOADING_H