/*
*	sns_ct7117x_sensor_instance_island.c
*	Island mode functions (code and data) for the sensor instance
*
*/

/*==============================================================================
  Include Files
  ============================================================================*/
#include "sns_mem_util.h"
#include "sns_sensor_instance.h"
#include "sns_service_manager.h"
#include "sns_stream_service.h"
#include "sns_rc.h"
#include "sns_request.h"
#include "sns_time.h"
#include "sns_sensor_event.h"
#include "sns_types.h"

#include "sns_ct7117x_hal.h"
#include "sns_ct7117x_sensor.h"
#include "sns_ct7117x_sensor_instance.h"

#include "sns_interrupt.pb.h"
#include "sns_async_com_port.pb.h"
#include "sns_timer.pb.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "sns_pb_util.h"
#include "sns_async_com_port_pb_utils.h"
#include "sns_diag_service.h"
#include "sns_std_event_gated_sensor.pb.h"
#include "sns_diag.pb.h"
#include "sns_sync_com_port_service.h"

/*==============================================================================
  Function Definitions
  ============================================================================*/
/*
*	process instance notify event
*/
static sns_rc ct7117x_temp_inst_notify_event(sns_sensor_instance * const this)
{
  ct7117x_instance_state *state = (ct7117x_instance_state*) this->state->state;
  sns_sensor_event *event;

  /* Turn COM port ON */
  state->scp_service->api->sns_scp_update_bus_power(
      state->com_port_info.port_handle, true);
  
  /*handle timer events*/
  if (state->temperature_timer_data_stream != NULL) 
  {
    event = state->temperature_timer_data_stream->api->peek_input(state->temperature_timer_data_stream);
    while (NULL != event) {
      pb_istream_t stream = pb_istream_from_buffer((pb_byte_t*)event->event,event->event_len);
      sns_timer_sensor_event timer_event;
      if (pb_decode(&stream, sns_timer_sensor_event_fields, &timer_event)) 
	  {
        if (event->message_id == SNS_TIMER_MSGID_SNS_TIMER_SENSOR_EVENT)
		{
          if(state->temperature_info.timer_is_active &&
            state->temperature_info.sampling_intvl > 0) 
          {            
            ct7117x_handle_temperature_data_stream_timer_event(this);
          }
        }
      } else {
      }
      event = state->temperature_timer_data_stream->api->get_next_input(state->temperature_timer_data_stream);
    }
  }

  if (state->interrupt_data_stream != NULL)
  {
    event = state->interrupt_data_stream->api->peek_input(state->interrupt_data_stream);
    while (NULL != event)
    {
      if (event->message_id == SNS_INTERRUPT_MSGID_SNS_INTERRUPT_EVENT)
      {
        sns_interrupt_event irq_event = sns_interrupt_event_init_default;
        pb_istream_t stream = pb_istream_from_buffer((pb_byte_t*)event->event, event->event_len);
        if (pb_decode(&stream, sns_interrupt_event_fields, &irq_event))
        {
          ct7117x_handle_interrupt_event(this, irq_event.timestamp);
        }
      }
      event = state->interrupt_data_stream->api->get_next_input(state->interrupt_data_stream);
    }
  }

  // Turn COM port OFF
  state->scp_service->api->sns_scp_update_bus_power(
      state->com_port_info.port_handle, false);
  return SNS_RC_SUCCESS;
}

/** Public Data Definitions. */
sns_sensor_instance_api ct7117x_sensor_instance_api =
{
  .struct_len = sizeof(sns_sensor_instance_api),
  .init = &ct7117x_temp_inst_init,
  .deinit = &ct7117x_temp_inst_deinit,
  .set_client_config = &ct7117x_temp_inst_set_client_config,
  .notify_event = &ct7117x_temp_inst_notify_event
};
