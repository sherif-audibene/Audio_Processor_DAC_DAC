# WiFi Configuration Guide

## Security Notice

WiFi credentials are now stored in the `sdkconfig` file, which is **excluded from git** to prevent committing sensitive information to version control.

## Setting WiFi Credentials

You have two options to configure your WiFi SSID and password:

### Option 1: Using menuconfig (Recommended)

1. Run the configuration menu:
   ```bash
   idf.py menuconfig
   ```

2. Navigate to: **WiFi Configuration**
   - Set `WiFi SSID` to your network name
   - Set `WiFi Password` to your network password

3. Save and exit (press `S` then `Q`)

4. Build and flash:
   ```bash
   idf.py build flash
   ```

### Option 2: Using sdkconfig.defaults file

1. Copy the example file:
   ```bash
   cp sdkconfig.defaults.example sdkconfig.defaults
   ```

2. Edit `sdkconfig.defaults` and set your credentials:
   ```
   CONFIG_WIFI_SSID="Your-Actual-SSID"
   CONFIG_WIFI_PASSWORD="Your-Actual-Password"
   ```

3. Build the project (sdkconfig.defaults will be merged automatically):
   ```bash
   idf.py build flash
   ```

### Option 3: Direct sdkconfig editing

You can also directly edit `sdkconfig` and set:
```
CONFIG_WIFI_SSID="Your-SSID"
CONFIG_WIFI_PASSWORD="Your-Password"
```

**Note:** The `sdkconfig` file is in `.gitignore` and will not be committed to git.

## Verifying Configuration

After building, you can verify your WiFi settings are configured by checking the build output or running:
```bash
idf.py menuconfig
```
and navigating to the WiFi Configuration section.

## For Team Development

If you're working in a team:
- Each developer should set their own WiFi credentials locally
- Never commit `sdkconfig` to git (it's already in `.gitignore`)
- Use `sdkconfig.defaults.example` as a template for documentation
- Consider using different WiFi networks for different environments (development, testing, production)

