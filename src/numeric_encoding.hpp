#ifndef NUMERIC_ENCODING_HPP
#define NUMERIC_ENCODING_HPP

#include <cmath>
#include "port_encoding.hpp"

using NumericPayload = ap_uint<NUMERIC_PAYLOAD_BIT_COUNT>;
using NumericTag = ap_uint<NUMERIC_TAG_BIT_COUNT>;
using NumericValue = ap_uint<NUMERIC_VALUE_BIT_COUNT>;

static const NumericValue NUMERIC_PAYLOAD_MASK = (NumericValue(1) << NUMERIC_PAYLOAD_BIT_COUNT) - 1;
static const NumericValue NUMERIC_TAG_MASK = (NumericValue(1) << NUMERIC_TAG_BIT_COUNT) - 1;

// HVM2 numeric tags.
static const int NUMERIC_TAG_SYMBOL = 0x00;
static const int NUMERIC_TAG_U24 = 0x01;
static const int NUMERIC_TAG_I24 = 0x02;
static const int NUMERIC_TAG_F24 = 0x03;
static const int NUMERIC_TAG_OPERATION_ADD = 0x04;
static const int NUMERIC_TAG_OPERATION_SUBTRACT = 0x05;
static const int NUMERIC_TAG_OPERATION_FLIPPED_SUBTRACT = 0x06;
static const int NUMERIC_TAG_OPERATION_MULTIPLY = 0x07;
static const int NUMERIC_TAG_OPERATION_DIVIDE = 0x08;
static const int NUMERIC_TAG_OPERATION_FLIPPED_DIVIDE = 0x09;
static const int NUMERIC_TAG_OPERATION_REMAINDER = 0x0A;
static const int NUMERIC_TAG_OPERATION_FLIPPED_REMAINDER = 0x0B;
static const int NUMERIC_TAG_OPERATION_EQUAL = 0x0C;
static const int NUMERIC_TAG_OPERATION_NOT_EQUAL = 0x0D;
static const int NUMERIC_TAG_OPERATION_LESS_THAN = 0x0E;
static const int NUMERIC_TAG_OPERATION_GREATER_THAN = 0x0F;
static const int NUMERIC_TAG_OPERATION_AND = 0x10;
static const int NUMERIC_TAG_OPERATION_OR = 0x11;
static const int NUMERIC_TAG_OPERATION_XOR = 0x12;
static const int NUMERIC_TAG_OPERATION_SHIFT_LEFT = 0x13;
static const int NUMERIC_TAG_OPERATION_FLIPPED_SHIFT_LEFT = 0x14;
static const int NUMERIC_TAG_OPERATION_SHIFT_RIGHT = 0x15;
static const int NUMERIC_TAG_OPERATION_FLIPPED_SHIFT_RIGHT = 0x16;

static inline NumericValue make_numeric_value(NumericTag numeric_tag, NumericPayload payload) {
  return (NumericValue(payload) << NUMERIC_TAG_BIT_COUNT) | NumericValue(numeric_tag);
}

static inline Port make_u24_number_port(ap_uint<24> value) {
  return make_port(PORT_TAG_NUMBER, make_numeric_value(NUMERIC_TAG_U24, NumericPayload(value)));
}

static inline Port make_i24_number_port(ap_int<24> value) {
  return make_port(PORT_TAG_NUMBER, make_numeric_value(NUMERIC_TAG_I24, NumericPayload(value)));
}

static inline Port make_f24_number_port(float value) {
  union {
    float floating_value;
    unsigned int integer_value;
  } raw_bits;
  raw_bits.floating_value = value;
  unsigned int shifted_bits = raw_bits.integer_value >> 8;
  unsigned int lost_bits = raw_bits.integer_value & 0xFF;
  shifted_bits += (!std::isnan(value)) &
                  ((lost_bits - ((lost_bits >> 7) & !(shifted_bits & 1U))) >> 7);
  shifted_bits |= std::isnan(value) ? 1U : 0U;
  return make_port(PORT_TAG_NUMBER, make_numeric_value(NUMERIC_TAG_F24, NumericPayload(shifted_bits)));
}

static inline Port make_numeric_operation_port(NumericTag operation_tag, NumericPayload captured_payload) {
  return make_port(PORT_TAG_NUMBER, make_numeric_value(operation_tag, captured_payload));
}

static inline NumericTag get_numeric_tag_from_port(Port numeric_port) {
  NumericValue numeric_value = get_port_value(numeric_port);
  return NumericTag(numeric_value & NUMERIC_TAG_MASK);
}

static inline NumericPayload get_numeric_payload_from_port(Port numeric_port) {
  NumericValue numeric_value = get_port_value(numeric_port);
  return NumericPayload((numeric_value >> NUMERIC_TAG_BIT_COUNT) & NUMERIC_PAYLOAD_MASK);
}

static inline ap_uint<1> is_numeric_type_tag(NumericTag numeric_tag) {
  return numeric_tag == NUMERIC_TAG_U24 ||
         numeric_tag == NUMERIC_TAG_I24 ||
         numeric_tag == NUMERIC_TAG_F24;
}

static inline ap_uint<1> is_numeric_operation_tag(NumericTag numeric_tag) {
  return numeric_tag >= NUMERIC_TAG_OPERATION_ADD &&
         numeric_tag <= NUMERIC_TAG_OPERATION_FLIPPED_SHIFT_RIGHT;
}

static inline ap_uint<1> is_supported_numeric_operation_tag(NumericTag numeric_tag) {
  return is_numeric_operation_tag(numeric_tag);
}

static inline ap_uint<1> is_u24_number(Port numeric_port) {
  return get_numeric_tag_from_port(numeric_port) == NUMERIC_TAG_U24;
}

static inline ap_uint<1> is_i24_number(Port numeric_port) {
  return get_numeric_tag_from_port(numeric_port) == NUMERIC_TAG_I24;
}

static inline ap_uint<1> is_f24_number(Port numeric_port) {
  return get_numeric_tag_from_port(numeric_port) == NUMERIC_TAG_F24;
}

static inline ap_uint<1> is_supported_number(Port numeric_port) {
  NumericTag numeric_tag = get_numeric_tag_from_port(numeric_port);
  return numeric_tag == NUMERIC_TAG_U24 ||
         numeric_tag == NUMERIC_TAG_I24 ||
         numeric_tag == NUMERIC_TAG_F24;
}

static inline ap_uint<1> is_supported_numeric_operation(Port numeric_port) {
  return is_supported_numeric_operation_tag(get_numeric_tag_from_port(numeric_port));
}

static inline ap_uint<24> get_u24_from_numeric_port(Port numeric_port) {
  return ap_uint<24>(get_numeric_payload_from_port(numeric_port));
}

static inline ap_int<24> get_i24_from_numeric_port(Port numeric_port) {
  return ap_int<24>(get_numeric_payload_from_port(numeric_port));
}

static inline float get_f24_from_numeric_port(Port numeric_port) {
  union {
    float floating_value;
    unsigned int integer_value;
  } raw_bits;
  raw_bits.integer_value = (static_cast<unsigned int>(get_numeric_payload_from_port(numeric_port)) << 8);
  return raw_bits.floating_value;
}

#endif
