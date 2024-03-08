
#include <stdint.h>


// Relay control constants ////////////////////////////////////////////////
const int RELAY_PIN = 12;    // The +control terminal of the SSR is soldered to pin D12 of the shield

const int PERIOD_MS = 2000;  // Repetition period during timed mode [milliseconds]
const int RELAY_ON_MS = 100; // How long to leave relay on [ms]
///////////////////////////////////////////////////////////////////////////



// Alarm signal sampling constants ////////////////////////////////////////
const int ALARM_PIN = 0;  // The alarm line is soldered to pin A0 of the shield

const int SAMPLING_DELAY_MS  = 1260;  // How long [ms] to delay before reading alarm line
// On dispensers tested so far, the alarm signal, if any, will start 1250 ms after the
// activation signal that initiates the first attempt to dispense.

const int SAMPLING_WINDOW_MS = 130;   // How long [ms] to sample alarm line after delay
// On dispensers tested so far, the alarm signal lasts for 150 ms.

// Choosing 1260 and 130 ms for these values means we'll sample just the middle 130 ms
// of the possible alarm period, with 10 ms buffers of unsampled time at the start and
// end of that period.


// Possible states for dispense status indicator
const char UNTRIED     = 'U';  // Before first attempt to dispense
const char IN_PROGRESS = 'I';  // After dispense attempt and before reading alarm line
const char FAILURE     = 'F';  // Alarm line indicated a failure to dispense
const char SUCCESS     = 'S';  // No signal on alarm line after dispense attempt
///////////////////////////////////////////////////////////////////////////


// Global flag that gets updated to track the status of dispense operations.
char dispense_status = UNTRIED;

char out_str[70]; // For use when formatting output strings with sprintf()
bool interactive = true;   // If false, start in timed mode



void setup() {
  Serial.begin(9600, SERIAL_8N1); // 8N1 is the default: 8 data bits, no parity, one stop bit
  pinMode(RELAY_PIN, OUTPUT);
}

void report_status() {
    sprintf(out_str, "Status: %c", dispense_status);
    Serial.println(out_str);
}

void attempt_dispense() {
  // Because this Arduino sketch only does one thing, it's fine to use delay()
  // and block execution while the dispense and alarm checking proceed.

  // This funtionality could be rewritten later in a non-blocking way, if necessary.
  // In that case, this example could be useful:
  // https://www.arduino.cc/en/Tutorial/BlinkWithoutDelay


  // If using a non-blocking approach, record activation time in millis
  //uint32_t activation_time = millis();

  dispense_status = IN_PROGRESS;
  digitalWrite(RELAY_PIN, HIGH);
  delay(RELAY_ON_MS);
  digitalWrite(RELAY_PIN, LOW);

  // Wait until alarm signal, if any, is available to read.
  // Dispenser activates when control line is grounded, not when it is released
  // from ground, so subtract the relay-on duration to get the correct additional delay.
  delay(SAMPLING_DELAY_MS - RELAY_ON_MS);

  uint32_t sampling_start_ms = millis();
  uint32_t curr_ms = millis();
  uint16_t aval; // analog value

  // Variables that record the number of samples and sum the sample values,
  // both during the alarm window and after, when the signal returns to normal.
  uint16_t n_samp_alarm = 0;
  uint16_t n_samp_normal = 0;
  uint32_t alarm_window_sum = 0;
  uint32_t normal_window_sum = 0;

  //Serial.println("\n\n\nDuring alarm signal window:\n");
  while((curr_ms - sampling_start_ms) < SAMPLING_WINDOW_MS) {
    curr_ms = millis();
    aval = analogRead(ALARM_PIN);
    alarm_window_sum += aval;
    n_samp_alarm++;
    //Serial.println(aval);

    //sprintf(out_str, "sample_t = %d, analog value = %d\n", curr_ms-sampling_start_ms, aval);
    //Serial.write(out_str);
  }

  delay(20); // Delay for the remaining 10 ms of alarm period, and 10 more ms into the post-alarm period
  //Serial.println("\nAfter alarm signal window:\n");

  sampling_start_ms = millis();
  curr_ms = millis();
  while((curr_ms - sampling_start_ms) < SAMPLING_WINDOW_MS) {
    curr_ms = millis();
    aval = analogRead(ALARM_PIN);

    normal_window_sum += aval;
    n_samp_normal++;
    //Serial.println(aval);

    //sprintf(out_str, "sample_t = %d, analog value = %d\n", curr_ms-sampling_start_ms, aval);
    //Serial.write(out_str);
  }

  // String operations are slow, so more data points can be sampled if we
  // wait until all the sampling is done before we try to print any output.
  //sprintf(out_str, "During: %05u samples, sum = %012lu", n_samp_alarm, alarm_window_sum);
  //Serial.println(out_str);
  //sprintf(out_str, "After:  %05u samples, sum = %012lu", n_samp_normal, normal_window_sum);
  //Serial.println(out_str);

  // Set flag indicating success or failure of dispense attempt
  //
  if((alarm_window_sum / n_samp_alarm) < 2) {
    dispense_status = FAILURE;
  } else {
    dispense_status = SUCCESS;
  }
}




char poll_serial() {
  char input_char = '\0';
  if (Serial.available() > 0)
    input_char = Serial.read();

  // If it's a lowercase letter, convert to uppercase
  if ('a' <= input_char && input_char <= 'z')
    input_char = 'A' + (input_char - 'a');

  return input_char;
}


void process_command(char cmd_char) {
  // Given a command character, run the appropriate function

  switch (cmd_char) {
    case '\0':
      // Null character means no input command to process
    break;

    case 'A':
      attempt_dispense();
    break;

    case 'S':
      report_status();
    break;

    case 'T': // Toggle between interactive and timed modes of operation
      if (interactive)
        interactive = false;
      else
        interactive = true;
    break;

    default:
      sprintf(out_str, "Unrecognized command char: '%c'\n", cmd_char);
      Serial.write(out_str);
  }
}


void loop() {
  char input_char = poll_serial();
  if (input_char) {
    process_command(input_char);
  }

  if (!interactive) { // If we're in timed mode, just run this simple sequence:
    //Serial.println("Running in timed mode. Send 'T' to toggle into interactive mode.");
    sprintf(out_str, "%012lu - timed mode, attempting to dispense\n", millis());
    Serial.write(out_str);
    attempt_dispense();
    report_status();
    delay(1000);

    // These delays block execution, so one may have to wait almost a full PERIOD_MS
    // between sending the mode toggle signal to the Arduino and it taking effect.
    // TODO: rewrite as non-blocking
  }
}



