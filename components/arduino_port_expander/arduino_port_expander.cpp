#include "arduino_port_expander.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace arduino_port_expander {

static const char *const TAG = "arduino_port_expander";

static const uint32_t CONFIGURE_TIMEOUT_MS = 5000;
static const uint32_t SETUP_TIMEOUT_MS = 15000;

static const uint8_t DIRECT_COMMAND = 0x96;
static const uint8_t ANTFREQ = 3;

static const uint8_t CMD_DIGITAL_READ = 0;
static const uint8_t CMD_WRITE_ANALOG = 2;
static const uint8_t CMD_WRITE_DIGITAL_HIGH = 3;
static const uint8_t CMD_WRITE_DIGITAL_LOW = 4;
static const uint8_t CMD_SETUP_PIN_OUTPUT = 5;
static const uint8_t CMD_SETUP_PIN_INPUT_PULLUP = 6;
static const uint8_t CMD_SETUP_PIN_INPUT = 7;
// 16 analog registers.. A0 to A15
// A4 and A5 on Arduino Uno not supported due to I2C
static const uint8_t CMD_ANALOG_READ_A0  = 0b1000;  // 0x8 = A0
// ....
static const uint8_t CMD_ANALOG_READ_A15 = 0b10111; // 0x17 = A15

static const uint8_t CMD_SETUP_ANALOG_INTERNAL = 0x10;
static const uint8_t CMD_SETUP_ANALOG_DEFAULT = 0x11;

float ArduinoPortExpanderComponent::get_setup_priority() const { return setup_priority::IO; }

void ArduinoPortExpanderComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ArduinoPortExpander at %#02x ...", this->address_);
  uint8_t data[9];
  uint32_t timeout = millis() + SETUP_TIMEOUT_MS;

  while (this->read_register(CMD_DIGITAL_READ, data, 9) != i2c::ERROR_OK) {
    App.feed_wdt();
    if (millis() > timeout) {
      ESP_LOGE(TAG, "ArduinoPortExpander not available at 0x%02X", this->address_);
      this->mark_failed();
      return;
    }
  };
  this->write_register(this->analog_reference_, nullptr, 0);
  ESP_LOGCONFIG(TAG, "Successfully configured.");
}
void ArduinoPortExpanderComponent::loop() { this->read_valid_ = this->read_gpio_(); }
void ArduinoPortExpanderComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ArduinoPortExpander:");
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with ArduinoPortExpander failed!");
  }
}
bool ArduinoPortExpanderComponent::digital_read(uint8_t pin) {
  if (!this->read_valid_)
    this->read_valid_ = read_gpio_();
  uint8_t bit = pin % 8;
  uint8_t value = 0;
  if(pin < 8){
    value = this->read_buffer_[0];
  }else if(pin < 16){
    value = this->read_buffer_[1];
  }else if(pin < 24){
    value = this->read_buffer_[2];
  }else if(pin < 32){
    value = this->read_buffer_[3];
  }else if(pin < 40){
    value = this->read_buffer_[4];
  }else if(pin < 48){
    value = this->read_buffer_[5];
  }else if(pin < 56){
    value = this->read_buffer_[6];
  }else if(pin < 64){
    value = this->read_buffer_[7];
  }else{
    value = this->read_buffer_[8];
  }
  return value & (1 << bit);
}
void ArduinoPortExpanderComponent::digital_write(uint8_t pin, bool value) {
  if (this->is_failed())
    return;
  this->write_register(value ? CMD_WRITE_DIGITAL_HIGH : CMD_WRITE_DIGITAL_LOW, &pin, 1);
}
void ArduinoPortExpanderComponent::pin_mode(uint8_t pin, gpio::Flags flags) {
  ESP_LOGD(TAG, "Setting pin %d mode %d", pin, flags);
  if (flags == gpio::FLAG_INPUT) {
    this->write_register(CMD_SETUP_PIN_INPUT, &pin, 1);
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLUP)) {
    this->write_register(CMD_SETUP_PIN_INPUT_PULLUP, &pin, 1);
  } else if (flags == gpio::FLAG_OUTPUT) {
    this->write_register(CMD_SETUP_PIN_OUTPUT, &pin, 1);
  } else {
    ESP_LOGE(TAG, "Invalid pin mode for pin %d", pin);
  }
  this->read_valid_ = false;
}
bool ArduinoPortExpanderComponent::read_gpio_() {
  bool success;
  success = (this->read_register(CMD_DIGITAL_READ, this->read_buffer_, 9) == i2c::ERROR_OK);

  if (!success) {
    this->status_set_warning();
    return false;
  }
  this->status_clear_warning();
  return true;
}

void ArduinoPortExpanderGPIOPin::setup() {
  pin_mode(flags_);
  this->setup_ = true;
}
void ArduinoPortExpanderGPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool ArduinoPortExpanderGPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void ArduinoPortExpanderGPIOPin::digital_write(bool value) {
  if (this->setup_)
    this->parent_->digital_write(this->pin_, value != this->inverted_);
}
std::string ArduinoPortExpanderGPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u via ArduinoPortExpander", pin_);
  return buffer;
}

float ArduinoPortExpanderComponent::analog_read(uint8_t pin) {
  uint16_t value;
  bool ok = (this->read_register(CMD_ANALOG_READ_A0 + pin, (uint8_t *) &value, 2));
  ESP_LOGV(TAG, "analog read pin: %d ok: %d value %d ", pin, ok, value);
  return value;
}

}  // namespace arduino_port_expander
}  // namespace esphome
