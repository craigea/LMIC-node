/* This is an a generic payload parser for LoRaWAN. It will work for any network server.
 ** The code find the "payload" variable, sent by your sensor, and parse it if exists.
 ** The content of payload variable is always an Hexadecimal value.
 **
 ** Note: Additional variables can be created by the Network Server and sent directly to the bucket. Normally they aren't handled here.
 **
 ** Testing:
 ** You can do manual tests to the parse by using the Device Emulator. Copy and Paste the following JSON:
 ** [{ "variable": "data", "value": "0109611395" }]
 */

// Search the payload variable in the payload global variable. It's contents is always [ { variable, value...}, {variable, value...} ...]
const payload_raw = payload.find(
  (x) =>
    x.variable === "payload_raw" ||
    x.variable === "payload" ||
    x.variable === "data"
);
if (payload_raw) {
  try {
    // Convert the data from Hex to Javascript Buffer.
    const buffer = Buffer.from(payload_raw.value, "hex");

    // Payload is 6 bytes.
    // 0,1 - mAmp
    // 2,3 - Temperature
    // 4,5 - Humidity
    // More information about buffers can be found here: https://nodejs.org/api/buffer.html
    const data = [
      { variable: "milliamp", value: buffer.readInt16BE(0) },
      {
        variable: "temperature",
        value: buffer.readInt16BE(2) / 100,
        unit: "°C",
      },
      { variable: "humidity", value: buffer.readUInt16BE(4) / 100, unit: "%" },
    ];

    // This will concat the content sent by your device with the content generated in this payload parser.
    // It also add the field "group" and "time" to it, copying from your sensor data.
    payload = payload.concat(
      data.map((x) => ({
        ...x,
        group: String(payload_raw.serie || payload_raw.group),
//        time: String(payload_raw.time),
      }))
    );
  } catch (e) {
    // Print the error to the Live Inspector.
    console.error(e);

    // Return the variable parse_error for debugging.
    payload = [{ variable: "parse_error", value: e.message }];
  }
}
