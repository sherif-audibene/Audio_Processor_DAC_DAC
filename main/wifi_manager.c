#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "WIFI_MANAGER";

// Private variables
static bool wifi_initialized = false;
static bool wifi_connected = false;
static char current_ip[16] = {0};
static wifi_connection_callback_t connection_callback = NULL;
static int reconnect_attempts = 0;

/**
 * @brief Get human-readable description of WiFi disconnect reason
 */
static const char* wifi_disconnect_reason_string(wifi_err_reason_t reason) {
    switch (reason) {
        case WIFI_REASON_UNSPECIFIED:
            return "Unspecified error";
        case WIFI_REASON_AUTH_EXPIRE:
            return "Authentication expired";
        case WIFI_REASON_AUTH_LEAVE:
            return "Authentication left";
        case WIFI_REASON_ASSOC_EXPIRE:
            return "Association expired";
        case WIFI_REASON_ASSOC_TOOMANY:
            return "Too many associations";
        case WIFI_REASON_NOT_AUTHED:
            return "Not authenticated";
        case WIFI_REASON_NOT_ASSOCED:
            return "Not associated";
        case WIFI_REASON_ASSOC_LEAVE:
            return "Association left";
        case WIFI_REASON_ASSOC_NOT_AUTHED:
            return "Association not authenticated";
        case WIFI_REASON_DISASSOC_PWRCAP_BAD:
            return "Disassociate - power capability bad";
        case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
            return "Disassociate - supported channel bad";
        case WIFI_REASON_IE_INVALID:
            return "Invalid IE";
        case WIFI_REASON_MIC_FAILURE:
            return "MIC failure";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
            return "4-way handshake timeout";
        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
            return "Group key update timeout";
        case WIFI_REASON_IE_IN_4WAY_DIFFERS:
            return "IE in 4-way differs";
        case WIFI_REASON_GROUP_CIPHER_INVALID:
            return "Group cipher invalid";
        case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
            return "Pairwise cipher invalid";
        case WIFI_REASON_AKMP_INVALID:
            return "AKMP invalid";
        case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
            return "Unsupported RSN IE version";
        case WIFI_REASON_INVALID_RSN_IE_CAP:
            return "Invalid RSN IE capability";
        case WIFI_REASON_802_1X_AUTH_FAILED:
            return "802.1X authentication failed";
        case WIFI_REASON_CIPHER_SUITE_REJECTED:
            return "Cipher suite rejected";
        case WIFI_REASON_BEACON_TIMEOUT:
            return "Beacon timeout";
        case WIFI_REASON_NO_AP_FOUND:
            return "No AP found";
        case WIFI_REASON_AUTH_FAIL:
            return "Authentication failed (wrong password?)";
        case WIFI_REASON_ASSOC_FAIL:
            return "Association failed";
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return "Handshake timeout";
        case WIFI_REASON_CONNECTION_FAIL:
            return "Connection failed";
        default:
            return "Unknown reason";
    }
}

// Event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi station started");
                reconnect_attempts = 0;
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi connected to AP");
                reconnect_attempts = 0;  // Reset on successful connection
                break;
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* disconnected = 
                    (wifi_event_sta_disconnected_t*) event_data;
                
                reconnect_attempts++;
                const char* reason_str = wifi_disconnect_reason_string(disconnected->reason);
                
                ESP_LOGE(TAG, "WiFi disconnected! Reason: %d (%s)", 
                         disconnected->reason, reason_str);
                ESP_LOGE(TAG, "SSID: %s, Attempt #%d", 
                         disconnected->ssid, reconnect_attempts);
                
                // Log specific common issues
                if (disconnected->reason == WIFI_REASON_AUTH_FAIL) {
                    ESP_LOGE(TAG, "*** AUTHENTICATION FAILED - Check WiFi password! ***");
                } else if (disconnected->reason == WIFI_REASON_NO_AP_FOUND) {
                    ESP_LOGE(TAG, "*** AP NOT FOUND - Check SSID and ensure router is on! ***");
                } else if (disconnected->reason == WIFI_REASON_BEACON_TIMEOUT) {
                    ESP_LOGE(TAG, "*** BEACON TIMEOUT - Router may be out of range or offline! ***");
                } else if (disconnected->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT) {
                    ESP_LOGE(TAG, "*** HANDSHAKE TIMEOUT - Password may be incorrect! ***");
                }
                
                wifi_connected = false;
                memset(current_ip, 0, sizeof(current_ip));
                
                // Notify callback if registered
                if (connection_callback != NULL) {
                    connection_callback(false, NULL);
                }
                
                // Add delay before reconnecting to avoid rapid retries
                vTaskDelay(pdMS_TO_TICKS(1000 * reconnect_attempts));  // Exponential backoff
                esp_wifi_connect();
                break;
            }
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            snprintf(current_ip, sizeof(current_ip), IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Got IP address: %s", current_ip);
            ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&event->ip_info.netmask));
            ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&event->ip_info.gw));
            wifi_connected = true;
            reconnect_attempts = 0;  // Reset on successful IP assignment
            
            // Notify callback if registered
            if (connection_callback != NULL) {
                connection_callback(true, current_ip);
            }
        }
    }
}

esp_err_t wifi_manager_init(const wifi_manager_config_t *config) {
    if (wifi_initialized) {
        ESP_LOGW(TAG, "WiFi manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing WiFi manager...");

    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Configure WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                                &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                                &wifi_event_handler, NULL));

    // Set WiFi mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Configure WiFi station
    wifi_config_t wifi_config = {0};
    if (config && config->ssid && config->password) {
        strncpy((char*)wifi_config.sta.ssid, config->ssid, sizeof(wifi_config.sta.ssid) - 1);
        wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';  // Ensure null termination
        strncpy((char*)wifi_config.sta.password, config->password, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';  // Ensure null termination
        
        ESP_LOGI(TAG, "Configuring WiFi with SSID: %s", wifi_config.sta.ssid);
        ESP_LOGI(TAG, "Password length: %zu characters", strlen((char*)wifi_config.sta.password));
    } else {
        // Default configuration
        strcpy((char*)wifi_config.sta.ssid, "Sherif-Home-2.4");
        strcpy((char*)wifi_config.sta.password, "password");
        ESP_LOGW(TAG, "Using default WiFi credentials (SSID: %s)", wifi_config.sta.ssid);
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_LOGI(TAG, "WiFi configuration set successfully");

    wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi manager initialized");
    return ESP_OK;
}

esp_err_t wifi_manager_start(void) {
    if (!wifi_initialized) {
        ESP_LOGE(TAG, "WiFi manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting WiFi...");
    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WiFi started, connecting to AP...");
    return ESP_OK;
}

void wifi_manager_stop(void) {
    if (wifi_initialized) {
        esp_wifi_stop();
        wifi_connected = false;
        memset(current_ip, 0, sizeof(current_ip));
        ESP_LOGI(TAG, "WiFi stopped");
    }
}

bool wifi_manager_is_connected(void) {
    return wifi_connected;
}

esp_err_t wifi_manager_get_ip(char *ip_str, size_t len) {
    if (!ip_str || len < 16) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!wifi_connected || strlen(current_ip) == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    
    strncpy(ip_str, current_ip, len - 1);
    ip_str[len - 1] = '\0';
    return ESP_OK;
}

void wifi_manager_cleanup(void) {
    wifi_manager_stop();
    
    if (wifi_initialized) {
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler);
        esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
        esp_wifi_deinit();
        wifi_initialized = false;
        connection_callback = NULL;
        ESP_LOGI(TAG, "WiFi manager cleaned up");
    }
}

void wifi_manager_set_connection_callback(wifi_connection_callback_t callback) {
    connection_callback = callback;
}

