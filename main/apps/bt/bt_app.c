#include "bt_app.h"
#include "../../app_manager.h"
#include "../../ui_styles.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"

// Static variables
static lv_obj_t *bt_screen = NULL;
static lv_obj_t *device_list = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *control_panel = NULL;

// Bluetooth device structure
typedef struct {
    char name[64];
    esp_bd_addr_t address;
    bool connected;
    bool is_audio_device;
} bt_device_t;

// Bluetooth state
static bt_device_t *devices = NULL;
static int device_count = 0;
static bool bt_scanning = false;
static bool a2dp_connected = false;
static esp_bd_addr_t connected_device_addr = {0};
static int avrcp_volume = 50; // Default volume

// Forward declarations
static void create_device_list(void);
static void device_item_event_cb(lv_event_t *e);
static void back_button_event_cb(lv_event_t *e);
static void scan_button_event_cb(lv_event_t *e);
static void connect_button_event_cb(lv_event_t *e);
static void disconnect_button_event_cb(lv_event_t *e);
static void volume_slider_event_cb(lv_event_t *e);
static void play_button_event_cb(lv_event_t *e);
static void pause_button_event_cb(lv_event_t *e);
static void next_button_event_cb(lv_event_t *e);
static void prev_button_event_cb(lv_event_t *e);

// Bluetooth callback functions
static void bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
static void bt_a2dp_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
static void bt_avrcp_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);

// Initialize Bluetooth
static esp_err_t init_bluetooth(void)
{
    esp_err_t ret;
    
    // Initialize Bluetooth controller
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE("BT_APP", "Failed to release BLE memory: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE("BT_APP", "Failed to initialize controller: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        ESP_LOGE("BT_APP", "Failed to enable controller: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize Bluedroid
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE("BT_APP", "Failed to initialize bluedroid: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE("BT_APP", "Failed to enable bluedroid: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register callbacks
    ret = esp_bt_gap_register_callback(bt_gap_cb);
    if (ret != ESP_OK) {
        ESP_LOGE("BT_APP", "Failed to register gap callback: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_a2d_register_callback(bt_a2dp_cb);
    if (ret != ESP_OK) {
        ESP_LOGE("BT_APP", "Failed to register a2dp callback: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_a2d_sink_init();
    if (ret != ESP_OK) {
        ESP_LOGE("BT_APP", "Failed to initialize a2dp sink: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_avrc_ct_init();
    if (ret != ESP_OK) {
        ESP_LOGE("BT_APP", "Failed to initialize avrcp: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_avrc_ct_register_callback(bt_avrcp_cb);
    if (ret != ESP_OK) {
        ESP_LOGE("BT_APP", "Failed to register avrcp callback: %s", esp_err_to_name(ret));
        return ret;
    }

    // Set discoverable and connectable mode
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    
    ESP_LOGI("BT_APP", "Bluetooth initialized successfully");
    return ESP_OK;
}

// GAP callback
static void bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_BT_GAP_DISC_RES_EVT:
            // Device discovered
            if (param->disc_res.num_prop > 0) {
                for (int i = 0; i < param->disc_res.num_prop; i++) {
                    if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_BDNAME) {
                        char *name = (char *)param->disc_res.prop[i].val;
                        ESP_LOGI("BT_APP", "Discovered device: %s", name);
                        
                        // Add to device list
                        device_count++;
                        devices = realloc(devices, device_count * sizeof(bt_device_t));
                        if (devices) {
                            strncpy(devices[device_count-1].name, name, sizeof(devices[device_count-1].name) - 1);
                            memcpy(devices[device_count-1].address, param->disc_res.bda, ESP_BD_ADDR_LEN);
                            devices[device_count-1].connected = false;
                            devices[device_count-1].is_audio_device = true; // Assume it's audio device
                            
                            // Update UI
                            if (device_list) {
                                create_device_list();
                            }
                        }
                    }
                }
            }
            break;
            
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            // Discovery state changed
            bt_scanning = (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED);
            if (status_label) {
                lv_label_set_text(status_label, bt_scanning ? "Scanning..." : "Scan Complete");
            }
            break;
            
        case ESP_BT_GAP_AUTH_CMPL_EVT:
            // Authentication complete
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI("BT_APP", "Authentication success: %s", param->auth_cmpl.device_name);
            } else {
                ESP_LOGE("BT_APP", "Authentication failed: %d", param->auth_cmpl.stat);
            }
            break;
            
        default:
            break;
    }
}

// A2DP callback
static void bt_a2dp_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT:
            // Connection state changed
            if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                ESP_LOGI("BT_APP", "A2DP connected");
                a2dp_connected = true;
                memcpy(connected_device_addr, param->conn_stat.remote_bda, ESP_BD_ADDR_LEN);
                
                // Update device status
                for (int i = 0; i < device_count; i++) {
                    if (memcmp(devices[i].address, connected_device_addr, ESP_BD_ADDR_LEN) == 0) {
                        devices[i].connected = true;
                        break;
                    }
                }
                
                // Update UI
                if (status_label) {
                    lv_label_set_text(status_label, "Connected");
                }
                if (control_panel) {
                    lv_obj_clear_flag(control_panel, LV_OBJ_FLAG_HIDDEN);
                }
            } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                ESP_LOGI("BT_APP", "A2DP disconnected");
                a2dp_connected = false;
                memset(connected_device_addr, 0, ESP_BD_ADDR_LEN);
                
                // Update device status
                for (int i = 0; i < device_count; i++) {
                    devices[i].connected = false;
                }
                
                // Update UI
                if (status_label) {
                    lv_label_set_text(status_label, "Disconnected");
                }
                if (control_panel) {
                    lv_obj_add_flag(control_panel, LV_OBJ_FLAG_HIDDEN);
                }
            }
            break;
            
        default:
            break;
    }
}

// AVRCP callback
static void bt_avrcp_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
    switch (event) {
        case ESP_AVRC_CT_CONNECTION_STATE_EVT:
            // AVRCP connection state changed
            if (param->conn_stat.connected) {
                ESP_LOGI("BT_APP", "AVRCP connected");
            } else {
                ESP_LOGI("BT_APP", "AVRCP disconnected");
            }
            break;
            
        case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
            // Pass-through command response
            ESP_LOGI("BT_APP", "AVRCP pass-through response: %d", param->psth_rsp.key_code);
            break;
            
        case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
            // Notification of change
            if (param->change_ntf.event_id == ESP_AVRC_RN_VOLUME_CHANGE) {
                avrcp_volume = param->change_ntf.event_parameter.volume;
                ESP_LOGI("BT_APP", "Volume changed: %d", avrcp_volume);
                
                // Update volume slider if visible
                if (control_panel && !lv_obj_has_flag(control_panel, LV_OBJ_FLAG_HIDDEN)) {
                    lv_obj_t *slider = lv_obj_get_child(control_panel, 0);
                    if (slider) {
                        lv_slider_set_value(slider, avrcp_volume, LV_ANIM_OFF);
                    }
                }
            }
            break;
            
        default:
            break;
    }
}

// Start Bluetooth device discovery
static void start_scan(void)
{
    // Clear previous devices
    if (devices) {
        free(devices);
        devices = NULL;
    }
    device_count = 0;
    
    // Start discovery
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
    
    if (status_label) {
        lv_label_set_text(status_label, "Scanning...");
    }
}

// Connect to a device
static void connect_to_device(esp_bd_addr_t addr)
{
    ESP_LOGI("BT_APP", "Connecting to device: %02X:%02X:%02X:%02X:%02X:%02X", 
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    
    esp_a2d_sink_connect(addr);
    
    if (status_label) {
        lv_label_set_text(status_label, "Connecting...");
    }
}

// Disconnect from current device
static void disconnect_device(void)
{
    if (a2dp_connected) {
        esp_a2d_sink_disconnect(connected_device_addr);
        
        if (status_label) {
            lv_label_set_text(status_label, "Disconnecting...");
        }
    }
}

// Send AVRCP command
static void send_avrcp_command(uint8_t cmd)
{
    if (a2dp_connected) {
        esp_avrc_ct_send_passthrough_cmd(0, cmd, ESP_AVRC_PT_CMD_STATE_PRESSED);
        esp_avrc_ct_send_passthrough_cmd(0, cmd, ESP_AVRC_PT_CMD_STATE_RELEASED);
    }
}

// Set volume - FIXED FUNCTION NAME
static void set_volume(uint8_t volume)
{
    if (a2dp_connected) {
        esp_avrc_ct_send_set_absolute_volume_cmd(0, volume); // Fixed function name
        avrcp_volume = volume;
    }
}

// Create device list UI
static void create_device_list(void)
{
    if (device_list) {
        lv_obj_del(device_list);
    }
    
    // Get the actual screen height
    lv_coord_t screen_height = lv_obj_get_height(bt_screen);
    lv_coord_t status_height = 25;
    
    // Create scrollable container for device list
    device_list = lv_obj_create(bt_screen);
    lv_obj_set_size(device_list, lv_pct(95), screen_height - 35 - status_height - 10); // Fixed: removed unused variable
    lv_obj_set_pos(device_list, 2, 35 + 5); // Fixed: removed unused variable
    lv_obj_set_style_bg_color(device_list, lv_color_hex(UI_COLOR_BG_DARK), 0);
    lv_obj_set_style_radius(device_list, 5, 0);
    lv_obj_set_style_pad_all(device_list, 8, 0);
    lv_obj_set_style_border_width(device_list, 1, 0);
    lv_obj_set_style_border_color(device_list, lv_color_hex(UI_COLOR_SECONDARY), 0);
    lv_obj_set_scroll_dir(device_list, LV_DIR_VER);
    
    // Add device items
    lv_coord_t y_pos = 0;
    for (int i = 0; i < device_count; i++) {
        lv_obj_t *item_btn = lv_btn_create(device_list);
        lv_obj_set_size(item_btn, lv_pct(95), 40);
        lv_obj_set_pos(item_btn, 0, y_pos);
        
        // Different colors for connected vs disconnected
        if (devices[i].connected) {
            lv_obj_set_style_bg_color(item_btn, lv_color_hex(0x4CAF50), 0); // Green for connected
        } else {
            lv_obj_set_style_bg_color(item_btn, lv_color_hex(UI_COLOR_ACCENT), 0);
        }
        
        lv_obj_set_style_radius(item_btn, 5, 0);
        lv_obj_add_event_cb(item_btn, device_item_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        
        // Create label with device name
        lv_obj_t *item_label = lv_label_create(item_btn);
        lv_label_set_text(item_label, devices[i].name);
        lv_obj_set_style_text_color(item_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
        lv_obj_align(item_label, LV_ALIGN_LEFT_MID, 10, 0);
        
        // Add connection status indicator
        lv_obj_t *status_indicator = lv_label_create(item_btn);
        lv_label_set_text(status_indicator, devices[i].connected ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_color(status_indicator, 
                                   devices[i].connected ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000), 0);
        lv_obj_align(status_indicator, LV_ALIGN_RIGHT_MID, -10, 0);
        
        y_pos += 45; // Move to next position
    }
    
    // Show message if no devices found
    if (device_count == 0) {
        lv_obj_t *empty_label = lv_label_create(device_list);
        lv_label_set_text(empty_label, "No devices found\nPress Scan to search");
        lv_obj_set_style_text_color(empty_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
        lv_obj_center(empty_label);
        lv_label_set_long_mode(empty_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(empty_label, lv_pct(90));
    }
}

// Create control panel UI
static void create_control_panel(void)
{
    if (control_panel) {
        lv_obj_del(control_panel);
    }
    
    // Get the actual screen height
    lv_coord_t screen_height = lv_obj_get_height(bt_screen);
    
    // Create control panel (hidden by default)
    control_panel = lv_obj_create(bt_screen);
    lv_obj_set_size(control_panel, lv_pct(95), 180);
    lv_obj_set_pos(control_panel, 2, screen_height - 185);
    lv_obj_set_style_bg_color(control_panel, lv_color_hex(UI_COLOR_BG_DARK), 0);
    lv_obj_set_style_radius(control_panel, 5, 0);
    lv_obj_set_style_pad_all(control_panel, 8, 0);
    lv_obj_set_style_border_width(control_panel, 1, 0);
    lv_obj_set_style_border_color(control_panel, lv_color_hex(UI_COLOR_SECONDARY), 0);
    lv_obj_add_flag(control_panel, LV_OBJ_FLAG_HIDDEN); // Hidden until connected
    
    // Volume slider
    lv_obj_t *vol_slider = lv_slider_create(control_panel);
    lv_obj_set_size(vol_slider, lv_pct(90), 20);
    lv_obj_align(vol_slider, LV_ALIGN_TOP_MID, 0, 10);
    lv_slider_set_value(vol_slider, avrcp_volume, LV_ANIM_OFF);
    lv_obj_add_event_cb(vol_slider, volume_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t *vol_label = lv_label_create(control_panel);
    lv_label_set_text(vol_label, "Volume");
    lv_obj_set_style_text_color(vol_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_align_to(vol_label, vol_slider, LV_ALIGN_OUT_TOP_MID, 0, -5);
    
    // Transport controls
    lv_obj_t *controls_container = lv_obj_create(control_panel);
    lv_obj_set_size(controls_container, lv_pct(90), 80);
    lv_obj_align(controls_container, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(controls_container, lv_color_hex(UI_COLOR_BG_DARK), 0);
    lv_obj_set_style_border_width(controls_container, 0, 0);
    lv_obj_set_flex_flow(controls_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Previous button
    lv_obj_t *prev_btn = lv_btn_create(controls_container);
    lv_obj_set_size(prev_btn, 50, 50);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(UI_COLOR_SECONDARY), 0);
    lv_obj_add_event_cb(prev_btn, prev_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, LV_SYMBOL_PREV);
    lv_obj_center(prev_label);
    
    // Play button
    lv_obj_t *play_btn = lv_btn_create(controls_container);
    lv_obj_set_size(play_btn, 50, 50);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_add_event_cb(play_btn, play_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *play_label = lv_label_create(play_btn);
    lv_label_set_text(play_label, LV_SYMBOL_PLAY);
    lv_obj_center(play_label);
    
    // Pause button
    lv_obj_t *pause_btn = lv_btn_create(controls_container);
    lv_obj_set_size(pause_btn, 50, 50);
    lv_obj_set_style_bg_color(pause_btn, lv_color_hex(0xFF9800), 0);
    lv_obj_add_event_cb(pause_btn, pause_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *pause_label = lv_label_create(pause_btn);
    lv_label_set_text(pause_label, LV_SYMBOL_PAUSE);
    lv_obj_center(pause_label);
    
    // Next button
    lv_obj_t *next_btn = lv_btn_create(controls_container);
    lv_obj_set_size(next_btn, 50, 50);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(UI_COLOR_SECONDARY), 0);
    lv_obj_add_event_cb(next_btn, next_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, LV_SYMBOL_NEXT);
    lv_obj_center(next_label);
    
    // Disconnect button
    lv_obj_t *disconnect_btn = lv_btn_create(control_panel);
    lv_obj_set_size(disconnect_btn, lv_pct(90), 30);
    lv_obj_align(disconnect_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(disconnect_btn, lv_color_hex(0xFF4444), 0);
    lv_obj_add_event_cb(disconnect_btn, disconnect_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *disconnect_label = lv_label_create(disconnect_btn);
    lv_label_set_text(disconnect_label, "Disconnect");
    lv_obj_center(disconnect_label);
}

// Event callbacks
static void device_item_event_cb(lv_event_t *e)
{
    int device_index = (int)(uintptr_t)lv_event_get_user_data(e);
    
    if (device_index < 0 || device_index >= device_count) return;
    
    ESP_LOGI("BT_APP", "Selected device: %s", devices[device_index].name);
    
    if (!devices[device_index].connected) {
        connect_to_device(devices[device_index].address);
    }
}

static void back_button_event_cb(lv_event_t *e)
{
    app_manager_go_home();
}

static void scan_button_event_cb(lv_event_t *e)
{
    start_scan();
}

static void connect_button_event_cb(lv_event_t *e)
{
    // For simplicity, connect to first available device
    if (device_count > 0) {
        for (int i = 0; i < device_count; i++) {
            if (!devices[i].connected && devices[i].is_audio_device) {
                connect_to_device(devices[i].address);
                break;
            }
        }
    }
}

static void disconnect_button_event_cb(lv_event_t *e)
{
    disconnect_device();
}

static void volume_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int volume = lv_slider_get_value(slider);
    set_volume(volume);
}

static void play_button_event_cb(lv_event_t *e)
{
    send_avrcp_command(ESP_AVRC_PT_CMD_PLAY);
}

static void pause_button_event_cb(lv_event_t *e)
{
    send_avrcp_command(ESP_AVRC_PT_CMD_PAUSE);
}

static void next_button_event_cb(lv_event_t *e)
{
    send_avrcp_command(ESP_AVRC_PT_CMD_FORWARD);
}

static void prev_button_event_cb(lv_event_t *e)
{
    send_avrcp_command(ESP_AVRC_PT_CMD_BACKWARD);
}

void create_bt_app(void)
{
    if (bt_screen) return;

    ESP_LOGI("BT_APP", "Creating Bluetooth screen");
    
    // Create screen
    bt_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(bt_screen, lv_color_hex(UI_COLOR_BG_DARK), 0);
    lv_obj_set_style_pad_all(bt_screen, 0, 0);

    // Title bar
    lv_obj_t *title_bar = lv_obj_create(bt_screen);
    lv_obj_set_size(title_bar, lv_pct(100), 35);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    // Back button
    lv_obj_t *back_btn = lv_btn_create(title_bar);
    lv_obj_set_size(back_btn, 45, 25);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(UI_COLOR_ACCENT), 0);
    lv_obj_set_style_radius(back_btn, 3, 0);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_color(back_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_10, 0);
    lv_obj_center(back_label);

    // Scan button
    lv_obj_t *scan_btn = lv_btn_create(title_bar);
    lv_obj_set_size(scan_btn, 55, 25);
    lv_obj_align(scan_btn, LV_ALIGN_LEFT_MID, 55, 0);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x4CAF50), 0); // Green
    lv_obj_set_style_radius(scan_btn, 3, 0);
    lv_obj_add_event_cb(scan_btn, scan_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *scan_label = lv_label_create(scan_btn);
    lv_label_set_text(scan_label, "Scan");
    lv_obj_set_style_text_color(scan_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(scan_label, &lv_font_montserrat_10, 0);
    lv_obj_center(scan_label);

    // Connect button
    lv_obj_t *connect_btn = lv_btn_create(title_bar);
    lv_obj_set_size(connect_btn, 65, 25);
    lv_obj_align(connect_btn, LV_ALIGN_LEFT_MID, 115, 0);
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x2196F3), 0); // Blue
    lv_obj_set_style_radius(connect_btn, 3, 0);
    lv_obj_add_event_cb(connect_btn, connect_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_set_style_text_color(connect_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(connect_label, &lv_font_montserrat_10, 0);
    lv_obj_center(connect_label);

    // Title
    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "Bluetooth");
    lv_obj_set_style_text_color(title, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -5);

    // Status label
    status_label = lv_label_create(title_bar);
    lv_obj_set_style_text_color(status_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_10, 0);
    lv_obj_align(status_label, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_label_set_text(status_label, "Disconnected");

    // Create device list and control panel
    create_device_list();
    create_control_panel();

    // Initialize Bluetooth
    esp_err_t ret = init_bluetooth();
    if (ret != ESP_OK) {
        ESP_LOGE("BT_APP", "Failed to initialize Bluetooth");
        lv_label_set_text(status_label, "BT Init Failed");
    } else {
        // Start scanning automatically
        start_scan();
    }

    // Link to app manager
    app_info_t* app_info = app_manager_get_app_info(APP_BLUETOOTH);
    if (app_info) {
        app_info->screen = bt_screen;
        ESP_LOGI("BT_APP", "Bluetooth screen linked to app manager");
    }
    
    ESP_LOGI("BT_APP", "Bluetooth app created successfully");
}

void destroy_bt_app(void)
{
    if (bt_screen) {
        ESP_LOGI("BT_APP", "Bluetooth app destroyed");
        
        // Free allocated memory
        if (devices) {
            free(devices);
            devices = NULL;
        }
        device_count = 0;
        
        // Clean up Bluetooth
        esp_a2d_sink_disconnect(connected_device_addr);
        esp_a2d_sink_deinit();
        esp_avrc_ct_deinit();
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        
        // Clear the app registry
        app_info_t* app_info = app_manager_get_app_info(APP_BLUETOOTH);
        if (app_info) {
            app_info->screen = NULL;
        }
        
        lv_obj_del(bt_screen);
        bt_screen = NULL;
        device_list = NULL;
        status_label = NULL;
        control_panel = NULL;
    }
}

lv_obj_t* bt_app_get_screen(void)
{
    return bt_screen;
}