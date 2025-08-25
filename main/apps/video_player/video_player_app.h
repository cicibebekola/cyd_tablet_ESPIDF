/**
 * @file video_player_app.h
 * @brief MJPEG Video Player Application for ESP32 LVGL Tablet
 * 
 * Provides functionality to play MJPEG video files on ESP32 with LVGL display.
 * Optimized for 320x240 displays with memory-constrained environments.
 * 
 * Features:
 * - MJPEG format support (.mjpeg, .mjpg files)
 * - Frame-by-frame playback with timing control
 * - Play/pause controls with progress bar
 * - Memory-safe frame buffering (50KB per frame limit)
 * - Time display (current/total)
 * - Automatic file format detection
 * 
 * MJPEG File Format Expected:
 * - Header: frame_count, fps, width, height (16 bytes)
 * - Frames: frame_size (4 bytes) + JPEG data (variable size)
 * 
 * @author Your Name
 * @date 2025
 */

#ifndef VIDEO_PLAYER_APP_H
#define VIDEO_PLAYER_APP_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum JPEG frame size in bytes
 * 
 * This limit prevents memory issues on ESP32 without PSRAM.
 * Frames larger than this will be rejected. Adjust based on 
 * your video quality and available memory.
 */
#define VIDEO_MAX_FRAME_SIZE    (50 * 1024)  // 50KB per frame

/**
 * @brief Maximum video file path length
 */
#define VIDEO_MAX_PATH_LEN      512

/**
 * @brief Default FPS if not specified in video header
 */
#define VIDEO_DEFAULT_FPS       30

/**
 * @brief Video player states
 * 
 * Represents the current state of the video player for
 * UI updates and playback control.
 */
typedef enum {
    VIDEO_STATE_STOPPED,    ///< Video is stopped/not loaded
    VIDEO_STATE_PLAYING,    ///< Video is currently playing
    VIDEO_STATE_PAUSED,     ///< Video is paused
    VIDEO_STATE_ERROR       ///< Error state (file not found, corrupt, etc.)
} video_state_t;

/**
 * @brief MJPEG file header structure
 * 
 * Expected at the beginning of MJPEG files to provide
 * video metadata for proper playback timing and progress.
 */
typedef struct {
    uint32_t frame_count;   ///< Total number of frames in video
    uint32_t fps;           ///< Frames per second for playback timing
    uint32_t width;         ///< Video frame width in pixels
    uint32_t height;        ///< Video frame height in pixels
} mjpeg_header_t;

/**
 * @brief Video playback statistics
 * 
 * Provides information about the current video and playback state
 * for debugging and UI display purposes.
 */
typedef struct {
    uint32_t total_frames;     ///< Total frames in current video
    uint32_t current_frame;    ///< Currently displayed frame number
    uint32_t fps;              ///< Current playback FPS
    video_state_t state;       ///< Current playback state
    uint32_t duration_seconds; ///< Total video duration in seconds
    uint32_t position_seconds; ///< Current playback position in seconds
} video_stats_t;

/**
 * @brief Set the video file path for the next video player creation
 * 
 * This function must be called before switching to the video player app
 * to specify which video file should be loaded. This allows the video
 * player to work with the app manager's parameterless create function.
 * 
 * @param file_path Full path to the MJPEG video file to play
 * @return void
 * 
 * @note The file_path must point to a valid MJPEG file with proper header.
 * @warning This will destroy any existing video player instance.
 */
void video_player_set_file_path(const char* file_path);

/**
 * @brief Create and initialize the video player application
 * 
 * Creates the video player screen, loads the video file specified by
 * the previous call to video_player_set_file_path(), and sets up
 * playback controls. The video is initially in stopped state.
 * 
 * @return void
 * 
 * @note Must call video_player_set_file_path() first to set the video file.
 * @warning This function will destroy any existing video player instance.
 */
void create_video_player_app(void);

/**
 * @brief Destroy the video player application and free resources
 * 
 * Stops video playback, closes files, frees allocated memory,
 * destroys timers, and cleans up all LVGL objects. Safe to call
 * multiple times.
 * 
 * @return void
 */
void destroy_video_player_app(void);

/**
 * @brief Get the main screen object of the video player
 * 
 * Returns the LVGL screen object for the video player application.
 * Used by the app manager for screen management and transitions.
 * 
 * @return lv_obj_t* Pointer to the video player screen object,
 *                   or NULL if not created
 */
lv_obj_t* video_player_app_get_screen(void);

/**
 * @brief Check if a file is a supported video file type
 * 
 * Determines if a file can be played by the video player based on
 * its file extension. Currently supports MJPEG formats.
 * 
 * @param filename Name of the file (including extension)
 * @return true if the file is a supported video format
 * @return false if the file is not supported
 * 
 * Supported extensions:
 * - .mjpeg (Motion JPEG)
 * - .mjpg (Motion JPEG, short form)
 */
bool video_player_is_supported_file(const char* filename);

/**
 * @brief Get the currently loaded video file path
 * 
 * Returns the path of the currently loaded video file, or NULL if
 * no video is loaded.
 * 
 * @return const char* Path to current video file, or NULL
 */
const char* video_player_get_current_file(void);

/**
 * @brief Get current video playback statistics
 * 
 * Provides detailed information about the current video and
 * playback state for UI updates or debugging.
 * 
 * @param stats Pointer to video_stats_t structure to fill
 * @return true if stats were successfully retrieved
 * @return false if no video is loaded or stats unavailable
 */
bool video_player_get_stats(video_stats_t* stats);

/**
 * @brief Start or resume video playback
 * 
 * Begins playing the loaded video from the current position.
 * If video is paused, resumes from pause point. If stopped,
 * starts from the beginning.
 * 
 * @return true if playback started successfully
 * @return false if no video loaded or error occurred
 */
bool video_player_play(void);

/**
 * @brief Pause video playback
 * 
 * Pauses the currently playing video at the current frame.
 * Can be resumed with video_player_play().
 * 
 * @return true if successfully paused
 * @return false if not currently playing
 */
bool video_player_pause(void);

/**
 * @brief Stop video playback and reset to beginning
 * 
 * Stops playback and resets the video position to frame 0.
 * 
 * @return true if successfully stopped
 * @return false if error occurred
 */
bool video_player_stop(void);

/**
 * @brief Seek to a specific frame in the video
 * 
 * Jumps to the specified frame number. Video will remain in
 * its current play/pause state.
 * 
 * @param frame_number Frame number to seek to (0-based)
 * @return true if seek was successful
 * @return false if frame number invalid or seek failed
 * 
 * @note Seeking may be slow due to sequential file reading
 */
bool video_player_seek_frame(uint32_t frame_number);

/**
 * @brief Seek to a specific time position in the video
 * 
 * Jumps to the specified time position in seconds.
 * 
 * @param seconds Time position in seconds
 * @return true if seek was successful
 * @return false if time position invalid or seek failed
 */
bool video_player_seek_time(uint32_t seconds);

/**
 * @brief Get current video player state
 * 
 * Returns the current playback state for UI updates.
 * 
 * @return video_state_t Current player state
 */
video_state_t video_player_get_state(void);

/**
 * @brief Set video playback speed (if supported)
 * 
 * Adjusts the playback speed by modifying the frame timer interval.
 * Speed of 1.0 = normal speed, 0.5 = half speed, 2.0 = double speed.
 * 
 * @param speed Playback speed multiplier (0.1 to 4.0 recommended)
 * @return true if speed change was successful
 * @return false if speed invalid or not supported
 * 
 * @note Very high speeds may cause frame drops on ESP32
 */
bool video_player_set_speed(float speed);

/**
 * @brief Create a test MJPEG video file for development/testing
 * 
 * Creates a test video file at /sdcard/test_video.mjpeg with:
 * - 60 frames (2 seconds at 30 FPS)
 * - 240x320 resolution
 * - Colorful animation (rainbow effect)
 * - Proper MJPEG header format
 * 
 * This function is useful for testing the video player without needing
 * to transfer real video files to the SD card.
 * 
 * @return void
 * 
 * @note Requires SD card to be mounted and writable
 * @note Will overwrite existing test_video.mjpeg file
 */
void video_player_create_test_file(void);

#ifdef __cplusplus
}
#endif

#endif // VIDEO_PLAYER_APP_H