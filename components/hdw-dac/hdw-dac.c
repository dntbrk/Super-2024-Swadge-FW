//==============================================================================
// Includes
//==============================================================================

#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/dac_continuous.h"
#include "hdw-dac.h"

//==============================================================================
// Defines
//==============================================================================

#define BUF_SIZE        2048
#define DMA_DESCRIPTORS 4

//==============================================================================
// Variables
//==============================================================================

/** The handle created for the DAC */
static dac_continuous_handle_t dac_handle;

/** A queue to move dac_event_data_t from the interrupt to the main loop*/
static QueueHandle_t que;

static fnDacCallback_t dacCb = NULL;
static uint8_t tmpDacBuf[BUF_SIZE];

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Callback for DAC conversion events
 *
 * @param handle [in] DAC channel handle, created from dac_continuous_new_channels()
 * @param event [in] DAC event data
 * @param user_data [in] User registered context, passed from dac_continuous_register_event_callback()
 * @return Whether a high priority task has been waken up by this callback function
 */
static bool IRAM_ATTR dac_on_convert_done_callback(dac_continuous_handle_t handle, const dac_event_data_t* event,
                                                   void* user_data)
{
    QueueHandle_t que = (QueueHandle_t)user_data;
    BaseType_t need_awoke;
    /* When the queue is full, drop the oldest item */
    if (xQueueIsQueueFullFromISR(que))
    {
        dac_event_data_t dummy;
        xQueueReceiveFromISR(que, &dummy, &need_awoke);
    }
    /* Send the event from callback */
    xQueueSendFromISR(que, event, &need_awoke);
    return need_awoke;
}

/**
 * @brief Initialize the DAC
 *
 * @param cb
 */
void initDac(fnDacCallback_t cb)
{
    /* Save the callback */
    dacCb = cb;

    /* Configure the DAC */
    dac_continuous_config_t cont_cfg = {
        .chan_mask = DAC_CHANNEL_MASK_CH0,   // This is GPIO_NUM_17
        .desc_num  = DMA_DESCRIPTORS,        // The number of DMA descriptors
        .buf_size  = BUF_SIZE,               // The size of each MDA buffer
        .freq_hz   = AUDIO_SAMPLE_RATE_HZ,   // The frequency of DAC conversion
        .offset    = 0,                      // DC Offset automatically applied
        .clk_src   = DAC_DIGI_CLK_SRC_APB,   // DAC_DIGI_CLK_SRC_APB is 77hz->MHz and always available
                                             // DAC_DIGI_CLK_SRC_APLL is 6Hz -> MHz but may be used by other peripherals
        .chan_mode = DAC_CHANNEL_MODE_SIMUL, // Doesn't matter for single channel output
    };

    /* Allocate continuous channels */
    ESP_ERROR_CHECK(dac_continuous_new_channels(&cont_cfg, &dac_handle));

    /* Create a queue to transport the interrupt event data */
    que = xQueueCreate(DMA_DESCRIPTORS, sizeof(dac_event_data_t));

    /* Register callbacks for conversion events */
    dac_event_callbacks_t cbs = {
        .on_convert_done = dac_on_convert_done_callback,
        .on_stop         = NULL,
    };
    ESP_ERROR_CHECK(dac_continuous_register_event_callback(dac_handle, &cbs, que));
}

/**
 * @brief TODO
 *
 */
void deinitDac(void)
{
    dacStop();
    ESP_ERROR_CHECK(dac_continuous_del_channels(dac_handle));
}

/**
 * @brief TODO
 *
 */
void dacStart(void)
{
    /* Enable and start the continuous channels */
    ESP_ERROR_CHECK(dac_continuous_enable(dac_handle));
    ESP_ERROR_CHECK(dac_continuous_start_async_writing(dac_handle));
}

/**
 * @brief TODO
 *
 */
void dacStop(void)
{
    /* Stop and disable the continuous channels */
    ESP_ERROR_CHECK(dac_continuous_stop_async_writing(dac_handle));
    ESP_ERROR_CHECK(dac_continuous_disable(dac_handle));
}

/**
 * @brief Poll the queue to see if it needs to be filled with audio samples
 */
void dacPoll(void)
{
    /* If there is an event to receive, receive it */
    dac_event_data_t evt_data;
    if (xQueueReceive(que, &evt_data, 0))
    {
        /* Ask the application to fill a buffer */
        dacCb(&tmpDacBuf, evt_data.buf_size);

        /* Write the data to the DAC */
        size_t loaded_bytes = 0;
        dac_continuous_write_asynchronously(dac_handle, evt_data.buf, evt_data.buf_size, //
                                            tmpDacBuf, evt_data.buf_size, &loaded_bytes);
        /* assume loaded_bytes == BUF_SIZE */
    }
}
