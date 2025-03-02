# Roblox Price Watcher v1.0

**Roblox Price Watcher v1.0** is a desktop application designed to monitor asset prices on Roblox, providing real-time updates and notifications via Discord webhooks when prices drop below user-defined thresholds. Built with ImGui and DirectX11, it offers an intuitive GUI for managing assets, setting thresholds, and debugging price checks.

## Features

- **Asset Price Monitoring**: Track the current prices of Roblox catalog items.
- **Price Threshold Alerts**: Receive Discord webhook notifications when an asset’s price falls below a set threshold.
- **Thumbnail Display**: View asset thumbnails fetched from Roblox’s API within the app.
- **Cookie Management**: Securely input and save your `.ROBLOSECURITY` cookie for API access.
- **Customizable Webhook**: Configure a Discord webhook URL for notifications.
- **Debugging Tools**: Built-in debug window with detailed logs and response data.
- **Dockable Interface**: ImGui-based docking system for a customizable layout with collapsible windows.

## Installation

### Prerequisites
- **Windows OS**: The application is designed for Windows due to its use of DirectX11 and Win32 APIs.
- **Visual Studio**: Required for building the project (e.g., VS 2019 or later with C++ support).
- **DirectX SDK**: Ensure DirectX 11 is installed (typically included with Windows SDK).
- **Dependencies**: 
  - ImGui library (assumed to be in `imgui_physical_store` folder).
  - Other libraries like `nlohmann/json` for JSON parsing (if applicable).

### Building from Source
1. **Clone the Repository**:
   ```bash
   git clone https://github.com/ProjectBoring/Roblox-Price-Watcher-v1.git
   cd Roblox-Price-Watcher-v1


![image](https://github.com/user-attachments/assets/5294fd23-6987-440c-a094-37fd63f5611e)
