#include "web_server.h"
#include "device_params.h"
#include "lcd_task.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "WEB_SERVER";

// Private variables
static httpd_handle_t server_handle = NULL;

// HTML page content
static const char* html_page = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta name='viewport' content='width=device-width, initial-scale=1'>"
"<title>ESP32-S3 Audio Processor Control</title>"
"<style>"
"* { box-sizing: border-box; }"
"body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f5f5f5; }"
".container { max-width: 800px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
"h1 { color: #333; margin-top: 0; }"
".section { margin-bottom: 30px; padding: 20px; background: #f9f9f9; border-radius: 5px; }"
".section h2 { color: #555; margin-top: 0; font-size: 1.2em; border-bottom: 2px solid #4CAF50; padding-bottom: 10px; }"
".form-group { margin-bottom: 15px; }"
"label { display: block; margin-bottom: 5px; color: #666; font-weight: bold; }"
"input[type='number'], input[type='range'] { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; }"
"input[type='checkbox'] { width: 20px; height: 20px; margin-right: 10px; }"
"button { background: #4CAF50; color: white; padding: 12px 24px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin-right: 10px; }"
"button:hover { background: #45a049; }"
"button.secondary { background: #2196F3; }"
"button.secondary:hover { background: #0b7dda; }"
".status { padding: 10px; margin: 10px 0; border-radius: 4px; }"
".status.success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }"
".status.error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }"
".range-value { display: inline-block; margin-left: 10px; color: #4CAF50; font-weight: bold; }"
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h1>🎵 ESP32-S3 Audio Processor Control Panel</h1>"
"<div id='status'></div>"
"<form id='configForm'>"
"<div class='section'>"
"<h2>Audio Settings</h2>"
"<div class='form-group'>"
"<label>Volume Scale: <span class='range-value' id='volumeValue'>1.5</span></label>"
"<input type='range' id='volumeScale' name='volume_scale' min='0' max='3' step='0.1' value='1.5' oninput='document.getElementById(\"volumeValue\").textContent=this.value'>"
"</div>"
"<div class='form-group'>"
"<label><input type='checkbox' id='enableDebug' name='enable_debug'> Enable Debug</label>"
"</div>"
"<div class='form-group'>"
"<label><input type='checkbox' id='enableChannelSwap' name='enable_channel_swap' checked> Enable Channel Swap</label>"
"</div>"
"<div class='form-group'>"
"<label><input type='checkbox' id='enableDelay' name='enable_delay' checked> Enable Delay Effect</label>"
"</div>"
"<div class='form-group'>"
"<label>Delay Time (ms): <span class='range-value' id='delayTimeValue'>50</span></label>"
"<input type='range' id='delayTime' name='delay_time_ms' min='0' max='1000' step='10' value='50' oninput='document.getElementById(\"delayTimeValue\").textContent=this.value'>"
"</div>"
"<div class='form-group'>"
"<label>Delay Mix: <span class='range-value' id='delayMixValue'>0.9</span></label>"
"<input type='range' id='delayMix' name='delay_mix' min='0' max='1' step='0.1' value='0.9' oninput='document.getElementById(\"delayMixValue\").textContent=this.value'>"
"</div>"
"<div class='form-group'>"
"<label>Delay Feedback: <span class='range-value' id='delayFeedbackValue'>0.4</span></label>"
"<input type='range' id='delayFeedback' name='delay_feedback' min='0' max='1' step='0.1' value='0.4' oninput='document.getElementById(\"delayFeedbackValue\").textContent=this.value'>"
"</div>"
"<div class='form-group'>"
"<label><input type='checkbox' id='enablePitchShift' name='enable_pitch_shift'> Enable Pitch Shift</label>"
"</div>"
"<div class='form-group'>"
"<label>Pitch Ratio: <span class='range-value' id='pitchRatioValue'>1.0</span> (1.0 = normal, 2.0 = octave up, 0.5 = octave down)</label>"
"<input type='range' id='pitchRatio' name='pitch_ratio' min='0.5' max='2.0' step='0.05' value='1.0' oninput='document.getElementById(\"pitchRatioValue\").textContent=this.value'>"
"</div>"
"</div>"
"<div class='section'>"
"<h2>LCD Display Settings</h2>"
"<div class='form-group'>"
"<label>Display Mode:</label>"
"<select id='displayMode' name='display_mode' style='width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px;'>"
"<option value='oscilloscope'>Oscilloscope (Waveform)</option>"
"<option value='spectrum'>Spectral Analyzer (FFT)</option>"
"<option value='stats'>System Stats (CPU/Memory)</option>"
"</select>"
"</div>"
"<div class='form-group'>"
"<label>Contrast: <span class='range-value' id='contrastValue'>20</span></label>"
"<input type='range' id='lcdContrast' name='lcd_contrast' min='0' max='63' step='1' value='20' oninput='document.getElementById(\"contrastValue\").textContent=this.value'>"
"</div>"
"<div class='form-group'>"
"<label>Samples Per Screen: <span class='range-value' id='samplesValue'>256</span></label>"
"<input type='range' id='samplesPerScreen' name='samples_per_screen' min='32' max='4096' step='1' value='256' oninput='document.getElementById(\"samplesValue\").textContent=this.value'>"
"</div>"
"<div class='form-group'>"
"<label>Amplitude Scale (%): <span class='range-value' id='amplitudeValue'>100</span></label>"
"<input type='range' id='amplitudeScale' name='amplitude_scale' min='10' max='200' step='5' value='100' oninput='document.getElementById(\"amplitudeValue\").textContent=this.value'>"
"</div>"
"<div class='form-group'>"
"<label><input type='checkbox' id='showGrid' name='show_grid' checked> Show Grid</label>"
"</div>"
"<div class='form-group'>"
"<label><input type='checkbox' id='showCenterLine' name='show_center_line' checked> Show Center Line</label>"
"</div>"
"</div>"
"<button type='button' onclick='loadConfig()'>Load Current Settings</button>"
"<button type='button' onclick='saveConfig()'>Save Settings</button>"
"<button type='button' class='secondary' onclick='resetConfig()'>Reset to Defaults</button>"
"</form>"
"</div>"
"<script>"
"function showStatus(message, isError) {"
"    const statusDiv = document.getElementById('status');"
"    statusDiv.className = 'status ' + (isError ? 'error' : 'success');"
"    statusDiv.textContent = message;"
"    setTimeout(() => statusDiv.textContent = '', 5000);"
"}"
"async function loadDisplayMode() {"
"    try {"
"        const response = await fetch('/api/display/mode');"
"        const data = await response.json();"
"        if (data.success) {"
"            document.getElementById('displayMode').value = data.mode;"
"        }"
"    } catch (error) {"
"        console.error('Error loading display mode:', error);"
"    }"
"}"
"async function saveDisplayMode() {"
"    const mode = document.getElementById('displayMode').value;"
"    try {"
"        const response = await fetch('/api/display/mode', {"
"            method: 'POST',"
"            headers: { 'Content-Type': 'application/json' },"
"            body: JSON.stringify({ mode: mode })"
"        });"
"        const data = await response.json();"
"        if (data.success) {"
"            showStatus('Display mode changed to ' + mode);"
"        } else {"
"            showStatus('Failed to change display mode: ' + (data.error || 'Unknown error'), true);"
"        }"
"    } catch (error) {"
"        showStatus('Error changing display mode: ' + error, true);"
"    }"
"}"
"async function loadConfig() {"
"    try {"
"        const response = await fetch('/api/config');"
"        const data = await response.json();"
"        if (data.success) {"
"            const cfg = data.config;"
"            document.getElementById('volumeScale').value = cfg.audio.volume_scale;"
"            document.getElementById('volumeValue').textContent = cfg.audio.volume_scale;"
"            document.getElementById('enableDebug').checked = cfg.audio.enable_debug;"
"            document.getElementById('enableChannelSwap').checked = cfg.audio.enable_channel_swap;"
"            document.getElementById('enableDelay').checked = cfg.audio.enable_delay;"
"            document.getElementById('delayTime').value = cfg.audio.delay_time_ms;"
"            document.getElementById('delayTimeValue').textContent = cfg.audio.delay_time_ms;"
"            document.getElementById('delayMix').value = cfg.audio.delay_mix;"
"            document.getElementById('delayMixValue').textContent = cfg.audio.delay_mix;"
"            document.getElementById('delayFeedback').value = cfg.audio.delay_feedback;"
"            document.getElementById('delayFeedbackValue').textContent = cfg.audio.delay_feedback;"
"            document.getElementById('enablePitchShift').checked = cfg.audio.enable_pitch_shift;"
"            document.getElementById('pitchRatio').value = cfg.audio.pitch_ratio;"
"            document.getElementById('pitchRatioValue').textContent = cfg.audio.pitch_ratio;"
"            document.getElementById('lcdContrast').value = cfg.lcd.lcd_contrast;"
"            document.getElementById('contrastValue').textContent = cfg.lcd.lcd_contrast;"
"            document.getElementById('samplesPerScreen').value = cfg.lcd.waveform.samples_per_screen;"
"            document.getElementById('samplesValue').textContent = cfg.lcd.waveform.samples_per_screen;"
"            document.getElementById('amplitudeScale').value = cfg.lcd.waveform.amplitude_scale;"
"            document.getElementById('amplitudeValue').textContent = cfg.lcd.waveform.amplitude_scale;"
"            document.getElementById('showGrid').checked = cfg.lcd.waveform.show_grid;"
"            document.getElementById('showCenterLine').checked = cfg.lcd.waveform.show_center_line;"
"            if (cfg.lcd.waveform.mode) {"
"                document.getElementById('displayMode').value = cfg.lcd.waveform.mode;"
"            }"
"            showStatus('Configuration loaded successfully');"
"        } else {"
"            showStatus('Failed to load configuration', true);"
"        }"
"    } catch (error) {"
"        showStatus('Error loading configuration: ' + error, true);"
"    }"
"    loadDisplayMode();"
"}"
"async function saveConfig() {"
"    const form = document.getElementById('configForm');"
"    const formData = new FormData(form);"
"    const config = {"
"        audio: {"
"            volume_scale: parseFloat(formData.get('volume_scale')),"
"            enable_debug: formData.has('enable_debug'),"
"            enable_channel_swap: formData.has('enable_channel_swap'),"
"            enable_delay: formData.has('enable_delay'),"
"            delay_time_ms: parseFloat(formData.get('delay_time_ms')),"
"            delay_mix: parseFloat(formData.get('delay_mix')),"
"            delay_feedback: parseFloat(formData.get('delay_feedback')),"
"            enable_pitch_shift: formData.has('enable_pitch_shift'),"
"            pitch_ratio: parseFloat(formData.get('pitch_ratio'))"
"        },"
"        lcd: {"
"            lcd_contrast: parseInt(formData.get('lcd_contrast')),"
"            waveform: {"
"                samples_per_screen: parseInt(formData.get('samples_per_screen')),"
"                amplitude_scale: parseInt(formData.get('amplitude_scale')),"
"                show_grid: formData.has('show_grid'),"
"                show_center_line: formData.has('show_center_line'),"
"                mode: formData.get('display_mode') || 'oscilloscope'"
"            }"
"        }"
"    };"
"    try {"
"        const response = await fetch('/api/config', {"
"            method: 'POST',"
"            headers: { 'Content-Type': 'application/json' },"
"            body: JSON.stringify(config)"
"        });"
"        const data = await response.json();"
"        if (data.success) {"
"            showStatus('Configuration saved successfully');"
"        } else {"
"            showStatus('Failed to save configuration: ' + (data.error || 'Unknown error'), true);"
"        }"
"    } catch (error) {"
"        showStatus('Error saving configuration: ' + error, true);"
"    }"
"}"
"async function resetConfig() {"
"    if (confirm('Reset all settings to defaults?')) {"
"        try {"
"            const response = await fetch('/api/config/reset', { method: 'POST' });"
"            const data = await response.json();"
"            if (data.success) {"
"                showStatus('Configuration reset to defaults');"
"                loadConfig();"
"            } else {"
"                showStatus('Failed to reset configuration', true);"
"            }"
"        } catch (error) {"
"            showStatus('Error resetting configuration: ' + error, true);"
"        }"
"    }"
"}"
"document.getElementById('displayMode').addEventListener('change', saveDisplayMode);"
"window.onload = function() { loadConfig(); };"
"</script>"
"</body>"
"</html>";

// Handler for GET /api/config
static esp_err_t api_get_config_handler(httpd_req_t *req) {
    device_params_t params;
    cJSON *json = cJSON_CreateObject();
    cJSON *audio_json = cJSON_CreateObject();
    cJSON *lcd_json = cJSON_CreateObject();
    cJSON *lcd_waveform_json = cJSON_CreateObject();
    
    if (device_params_get(&params) != ESP_OK) {
        cJSON_AddBoolToObject(json, "success", false);
        cJSON_AddStringToObject(json, "error", "Failed to get parameters");
    } else {
        cJSON_AddBoolToObject(json, "success", true);
        
        // Audio config
        cJSON_AddNumberToObject(audio_json, "volume_scale", params.audio.volume_scale);
        cJSON_AddBoolToObject(audio_json, "enable_debug", params.audio.enable_debug);
        cJSON_AddBoolToObject(audio_json, "enable_channel_swap", params.audio.enable_channel_swap);
        cJSON_AddBoolToObject(audio_json, "enable_delay", params.audio.enable_delay);
        cJSON_AddNumberToObject(audio_json, "delay_time_ms", params.audio.delay_time_ms);
        cJSON_AddNumberToObject(audio_json, "delay_mix", params.audio.delay_mix);
        cJSON_AddNumberToObject(audio_json, "delay_feedback", params.audio.delay_feedback);
        cJSON_AddBoolToObject(audio_json, "enable_pitch_shift", params.audio.enable_pitch_shift);
        cJSON_AddNumberToObject(audio_json, "pitch_ratio", params.audio.pitch_ratio);
        
        // LCD config
        cJSON_AddNumberToObject(lcd_json, "lcd_contrast", params.lcd.lcd_contrast);
        cJSON_AddNumberToObject(lcd_waveform_json, "samples_per_screen", params.lcd.waveform.samples_per_screen);
        cJSON_AddNumberToObject(lcd_waveform_json, "amplitude_scale", params.lcd.waveform.amplitude_scale);
        cJSON_AddBoolToObject(lcd_waveform_json, "show_grid", params.lcd.waveform.show_grid);
        cJSON_AddBoolToObject(lcd_waveform_json, "show_center_line", params.lcd.waveform.show_center_line);
        const char *mode_str;
        switch (params.lcd.waveform.mode) {
            case WAVEFORM_MODE_SPECTRUM: mode_str = "spectrum"; break;
            case WAVEFORM_MODE_STATS: mode_str = "stats"; break;
            default: mode_str = "oscilloscope"; break;
        }
        cJSON_AddStringToObject(lcd_waveform_json, "mode", mode_str);
        cJSON_AddItemToObject(lcd_json, "waveform", lcd_waveform_json);
        
        cJSON_AddItemToObject(json, "audio", audio_json);
        cJSON_AddItemToObject(json, "lcd", lcd_json);
    }
    
    char *json_str = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    cJSON_Delete(json);
    
    return ESP_OK;
}

// Handler for POST /api/config
static esp_err_t api_post_config_handler(httpd_req_t *req) {
    char content[1024];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    content[ret] = '\0';
    
    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Invalid JSON", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    device_params_t params;
    if (device_params_get(&params) != ESP_OK) {
        cJSON_Delete(json);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Parse audio config
    cJSON *audio_json = cJSON_GetObjectItem(json, "audio");
    if (audio_json) {
        cJSON *item = cJSON_GetObjectItem(audio_json, "volume_scale");
        if (item) params.audio.volume_scale = item->valuedouble;
        item = cJSON_GetObjectItem(audio_json, "enable_debug");
        if (item) params.audio.enable_debug = cJSON_IsTrue(item);
        item = cJSON_GetObjectItem(audio_json, "enable_channel_swap");
        if (item) params.audio.enable_channel_swap = cJSON_IsTrue(item);
        item = cJSON_GetObjectItem(audio_json, "enable_delay");
        if (item) params.audio.enable_delay = cJSON_IsTrue(item);
        item = cJSON_GetObjectItem(audio_json, "delay_time_ms");
        if (item) params.audio.delay_time_ms = item->valuedouble;
        item = cJSON_GetObjectItem(audio_json, "delay_mix");
        if (item) params.audio.delay_mix = item->valuedouble;
        item = cJSON_GetObjectItem(audio_json, "delay_feedback");
        if (item) params.audio.delay_feedback = item->valuedouble;
        item = cJSON_GetObjectItem(audio_json, "enable_pitch_shift");
        if (item) params.audio.enable_pitch_shift = cJSON_IsTrue(item);
        item = cJSON_GetObjectItem(audio_json, "pitch_ratio");
        if (item) params.audio.pitch_ratio = item->valuedouble;
    }
    
    // Parse LCD config
    cJSON *lcd_json = cJSON_GetObjectItem(json, "lcd");
    if (lcd_json) {
        cJSON *item = cJSON_GetObjectItem(lcd_json, "lcd_contrast");
        if (item) params.lcd.lcd_contrast = item->valueint;
        cJSON *waveform_json = cJSON_GetObjectItem(lcd_json, "waveform");
        if (waveform_json) {
            item = cJSON_GetObjectItem(waveform_json, "samples_per_screen");
            if (item) params.lcd.waveform.samples_per_screen = item->valueint;
            item = cJSON_GetObjectItem(waveform_json, "amplitude_scale");
            if (item) params.lcd.waveform.amplitude_scale = item->valueint;
            item = cJSON_GetObjectItem(waveform_json, "show_grid");
            if (item) params.lcd.waveform.show_grid = cJSON_IsTrue(item);
            item = cJSON_GetObjectItem(waveform_json, "show_center_line");
            if (item) params.lcd.waveform.show_center_line = cJSON_IsTrue(item);
            item = cJSON_GetObjectItem(waveform_json, "mode");
            if (item && cJSON_IsString(item)) {
                const char *mode_str = item->valuestring;
                if (strcmp(mode_str, "spectrum") == 0 || strcmp(mode_str, "spectral") == 0) {
                    params.lcd.waveform.mode = WAVEFORM_MODE_SPECTRUM;
                } else if (strcmp(mode_str, "stats") == 0) {
                    params.lcd.waveform.mode = WAVEFORM_MODE_STATS;
                } else {
                    params.lcd.waveform.mode = WAVEFORM_MODE_OSCILLOSCOPE;
                }
            }
        }
    }
    
    cJSON_Delete(json);
    
    esp_err_t err = device_params_update(&params);
    
    cJSON *response = cJSON_CreateObject();
    if (err == ESP_OK) {
        cJSON_AddBoolToObject(response, "success", true);
    } else {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "error", esp_err_to_name(err));
    }
    
    char *response_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_str, strlen(response_str));
    free(response_str);
    cJSON_Delete(response);
    
    return ESP_OK;
}

// Handler for POST /api/config/reset
static esp_err_t api_reset_config_handler(httpd_req_t *req) {
    device_params_t params;
    device_params_get_defaults(&params);
    
    esp_err_t err = device_params_update(&params);
    
    cJSON *response = cJSON_CreateObject();
    if (err == ESP_OK) {
        cJSON_AddBoolToObject(response, "success", true);
    } else {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "error", esp_err_to_name(err));
    }
    
    char *response_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_str, strlen(response_str));
    free(response_str);
    cJSON_Delete(response);
    
    return ESP_OK;
}

// Handler for POST /api/display/mode
static esp_err_t api_display_mode_handler(httpd_req_t *req) {
    char content[256];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    content[ret] = '\0';
    
    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Invalid JSON", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    cJSON *mode_item = cJSON_GetObjectItem(json, "mode");
    if (!mode_item || !cJSON_IsString(mode_item)) {
        cJSON_Delete(json);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Missing or invalid 'mode' field", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    const char *mode_str = mode_item->valuestring;
    waveform_mode_t mode;
    
    if (strcmp(mode_str, "oscilloscope") == 0 || strcmp(mode_str, "waveform") == 0) {
        mode = WAVEFORM_MODE_OSCILLOSCOPE;
    } else if (strcmp(mode_str, "spectrum") == 0 || strcmp(mode_str, "spectral") == 0) {
        mode = WAVEFORM_MODE_SPECTRUM;
    } else if (strcmp(mode_str, "stats") == 0) {
        mode = WAVEFORM_MODE_STATS;
    } else {
        cJSON_Delete(json);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Invalid mode. Use 'oscilloscope', 'spectrum', or 'stats'", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    cJSON_Delete(json);
    
    // Update waveform config with new mode
    waveform_config_t waveform_config;
    esp_err_t err = lcd_task_get_waveform_config(&waveform_config);
    if (err == ESP_OK) {
        waveform_config.mode = mode;
        err = lcd_task_set_waveform_config(&waveform_config);
        
        // Also update device params to persist the setting
        device_params_t params;
        if (device_params_get(&params) == ESP_OK) {
            params.lcd.waveform.mode = mode;
            device_params_update(&params);
        }
    }
    
    cJSON *response = cJSON_CreateObject();
    if (err == ESP_OK) {
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "mode", mode_str);
    } else {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "error", esp_err_to_name(err));
    }
    
    char *response_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_str, strlen(response_str));
    free(response_str);
    cJSON_Delete(response);
    
    return ESP_OK;
}

// Handler for GET /api/display/mode
static esp_err_t api_get_display_mode_handler(httpd_req_t *req) {
    waveform_config_t waveform_config;
    esp_err_t err = lcd_task_get_waveform_config(&waveform_config);
    
    cJSON *response = cJSON_CreateObject();
    if (err == ESP_OK) {
        cJSON_AddBoolToObject(response, "success", true);
        const char *mode_str;
        switch (waveform_config.mode) {
            case WAVEFORM_MODE_OSCILLOSCOPE:
                mode_str = "oscilloscope";
                break;
            case WAVEFORM_MODE_SPECTRUM:
                mode_str = "spectrum";
                break;
            case WAVEFORM_MODE_STATS:
                mode_str = "stats";
                break;
            default:
                mode_str = "oscilloscope";
                break;
        }
        cJSON_AddStringToObject(response, "mode", mode_str);
    } else {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "error", esp_err_to_name(err));
    }
    
    char *response_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_str, strlen(response_str));
    free(response_str);
    cJSON_Delete(response);
    
    return ESP_OK;
}

// Handler for GET /
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

esp_err_t web_server_init(void) {
    if (server_handle != NULL) {
        ESP_LOGW(TAG, "Web server already initialized");
        return ESP_OK;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    config.max_open_sockets = 7;
    
    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);
    
    if (httpd_start(&server_handle, &config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_handle, &root_uri);
        
        httpd_uri_t api_get_config_uri = {
            .uri = "/api/config",
            .method = HTTP_GET,
            .handler = api_get_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_handle, &api_get_config_uri);
        
        httpd_uri_t api_post_config_uri = {
            .uri = "/api/config",
            .method = HTTP_POST,
            .handler = api_post_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_handle, &api_post_config_uri);
        
        httpd_uri_t api_reset_config_uri = {
            .uri = "/api/config/reset",
            .method = HTTP_POST,
            .handler = api_reset_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_handle, &api_reset_config_uri);
        
        httpd_uri_t api_get_display_mode_uri = {
            .uri = "/api/display/mode",
            .method = HTTP_GET,
            .handler = api_get_display_mode_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_handle, &api_get_display_mode_uri);
        
        httpd_uri_t api_post_display_mode_uri = {
            .uri = "/api/display/mode",
            .method = HTTP_POST,
            .handler = api_display_mode_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_handle, &api_post_display_mode_uri);
        
        ESP_LOGI(TAG, "Web server started successfully");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to start web server");
    return ESP_FAIL;
}

esp_err_t web_server_start(void) {
    return web_server_init();
}

void web_server_stop(void) {
    if (server_handle != NULL) {
        httpd_stop(server_handle);
        server_handle = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
}

void web_server_cleanup(void) {
    web_server_stop();
}

