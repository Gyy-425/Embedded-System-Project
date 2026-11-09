#include "colour_protocol.h"
#include "comm_protocol.h"
#include <stddef.h>

static bool colour_protocol_result_valid(colour_result_t result)
{
    uint8_t reason = colour_result_reason(result);
    uint8_t id = colour_result_class(result);
    return ((result & 0xC0U) == 0U) &&
        (((reason == COLOUR_REASON_MATCH) && (id < COLOUR_CLASS_COUNT)) ||
         ((reason != COLOUR_REASON_MATCH) && (id == COLOUR_UNKNOWN)));
}

static void colour_protocol_word(uint8_t *p_output, uint32_t value)
{
    p_output[0] = (uint8_t)(value >> 24U);
    p_output[1] = (uint8_t)(value >> 16U);
    p_output[2] = (uint8_t)(value >> 8U);
    p_output[3] = (uint8_t)value;
}

uint16_t colour_protocol_encode(uint8_t *p_output, uint32_t session,
                                 const colour_report_t *p_report,
                                 bool is_snapshot)
{
    if ((p_output == NULL) || (p_report == NULL) || (session == 0U))
    {
        return 0U;
    }
    const colour_observation_t *p_sample = &p_report->observation;
    if ((p_sample->raw[0] > COLOUR_ADC_MAX) ||
        (p_sample->raw[1] > COLOUR_ADC_MAX) ||
        !colour_protocol_result_valid(p_sample->result[0]) ||
        !colour_protocol_result_valid(p_sample->result[1]))
    {
        return 0U;
    }
    p_output[0] = is_snapshot ? 0xD1U : 0xD0U;
    colour_protocol_word(&p_output[1], session);
    colour_protocol_word(&p_output[5], p_sample->sequence);
    colour_protocol_word(&p_output[9], p_sample->sampled_ms);
    colour_protocol_word(&p_output[13], p_report->dropped);
    p_output[17] = (uint8_t)(p_sample->raw[0] >> 4U);
    p_output[18] = (uint8_t)(((uint32_t)p_sample->raw[0] << 4U) |
                              ((uint32_t)p_sample->raw[1] >> 8U));
    p_output[19] = (uint8_t)p_sample->raw[1];
    p_output[20] = p_sample->result[0];
    p_output[21] = p_sample->result[1];
    p_output[22] = comm_protocol_crc(p_output, 22U);
    return COLOUR_PACKET_SIZE;
}
