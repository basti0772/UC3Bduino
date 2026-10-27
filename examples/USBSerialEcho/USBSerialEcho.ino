/* SPDX-License-Identifier: MIT */
void setup() { SerialUSB.begin(115200); } /* Baud is compatibility metadata. */

void loop()
{
    /* Peek before write: on TX backpressure retain the byte in RX. This also
     * propagates backpressure to the host when its OUT traffic outruns IN.
     */
    if (SerialUSB && SerialUSB.available()) {
        int value = SerialUSB.peek();
        if (value >= 0 && SerialUSB.write((uint8_t)value) == 1) { SerialUSB.read(); }
    }
}
