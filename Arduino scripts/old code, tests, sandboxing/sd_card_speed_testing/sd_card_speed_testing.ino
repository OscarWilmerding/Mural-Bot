#include <SPI.h>
#include <SD.h>

#define CHIP_SELECT_PIN 5          // Change as needed
#define DATA_FILE       "/test.txt"
#define INDEX_FILE      "/test.idx"

// Writes a chunk plus a comma to DATA_FILE, and records the offset in INDEX_FILE
void appendData(const char* dataFile, const char* indexFile, const String& dataChunk) {
  // 1) Open data file for append
  File datafile = SD.open(dataFile, FILE_APPEND);
  if (!datafile) {
    Serial.println("Failed to open data file for appending");
    return;
  }

  // 2) Get the offset where we will start writing
  //    (This is the length of the data file before writing the new chunk)
  unsigned long offset = datafile.size();
  
  // 3) Open index file for append
  File idxfile = SD.open(indexFile, FILE_APPEND);
  if (!idxfile) {
    Serial.println("Failed to open index file for appending");
    datafile.close();
    return;
  }

  // 4) Write this offset to the index file
  //    We'll write it as raw bytes
  idxfile.write((uint8_t*)&offset, sizeof(offset));
  idxfile.close();

  // 5) Now write the actual data chunk plus a comma to the data file
  datafile.print(dataChunk);
  datafile.print(",");
  datafile.close();
}

/**
 * Retrieves the nth comma‐separated chunk from DATA_FILE by:
 *   - Finding the nth offset in INDEX_FILE
 *   - Seeking to that offset in DATA_FILE
 *   - Reading until the next comma
 * Also prints how long the function took (in microseconds).
 */
String recallValue(const char* dataFile, const char* indexFile, int n) {
  unsigned long startMicros = micros();

  // 1) Calculate where the nth offset is stored in the index file
  //    Each offset is an unsigned long = 4 bytes (on most 32-bit systems)
  unsigned long indexPos = (n - 1) * sizeof(unsigned long);

  // 2) Open the index file and check validity
  File idxfile = SD.open(indexFile, FILE_READ);
  if (!idxfile) {
    Serial.println("Failed to open index file for reading");
    return "";
  }

  // 3) Check if indexPos is within the size of the index file
  if (indexPos + sizeof(unsigned long) > idxfile.size()) {
    idxfile.close();
    Serial.println("Recall out of range: index file too small.");
    return "";
  }

  // 4) Seek and read the offset
  idxfile.seek(indexPos);
  unsigned long dataOffset = 0;
  idxfile.read((uint8_t*)&dataOffset, sizeof(dataOffset));
  idxfile.close();

  // 5) Open the data file and seek to dataOffset
  File datafile = SD.open(dataFile, FILE_READ);
  if (!datafile) {
    Serial.println("Failed to open data file for reading");
    return "";
  }

  datafile.seek(dataOffset);

  // 6) Read characters until we encounter a comma or run out of file
  String result;
  while (datafile.available()) {
    char c = datafile.read();
    if (c == ',' || c == '\n' || c == '\r') {
      // We reached the end of this chunk
      break;
    }
    result += c;
  }
  datafile.close();

  // 7) Print how long it took
  unsigned long endMicros = micros();
  Serial.print("Recall took: ");
  Serial.print(endMicros - startMicros);
  Serial.println(" us");

  return result;
}



/**
 * Wipes both the data and index files by simply overwriting them with empty content.
 */
void wipeFiles(const char* dataFile, const char* indexFile) {
  // Overwrite data file
  File df = SD.open(dataFile, FILE_WRITE);
  if (df) df.close();

  // Overwrite index file
  File ix = SD.open(indexFile, FILE_WRITE);
  if (ix) ix.close();
}

// -------------------------------------------------------------------------------------
// Setup & Loop
// -------------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  Serial.println("Initializing SD card...");
  if (!SD.begin(CHIP_SELECT_PIN)) {
    Serial.println("SD initialization failed!");
    while (true) {}
  }
  Serial.println("SD initialization done.");

  delay(1000);

  // Print help
  Serial.println("Available commands:");
  Serial.println("  append <text>  - Append <text> to the file + index");
  Serial.println("  recall <n>     - Retrieve the nth comma‐delimited entry");
  Serial.println("  wipe           - Wipe (empty) the data and index files");
}

// Add this new function to your existing code
void appendFiftyEntries(const char* dataFile, const char* indexFile, const String& dataChunk) {
  for (int i = 0; i < 50; i++) {
    appendData(dataFile, indexFile, dataChunk);
  }
}

// Modify your loop function to include the new command
void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.startsWith("append ")) {
      String dataChunk = input.substring(7);
      appendData(DATA_FILE, INDEX_FILE, dataChunk);
      Serial.print("Appended: ");
      Serial.println(dataChunk);
    }
    else if (input.startsWith("recall ")) {
      int n = input.substring(7).toInt();
      String result = recallValue(DATA_FILE, INDEX_FILE, n);
      Serial.print("N = ");
      Serial.print(n);
      Serial.print(" => '");
      Serial.print(result);
      Serial.println("'");
    }
    else if (input.equalsIgnoreCase("wipe")) {
      wipeFiles(DATA_FILE, INDEX_FILE);
      Serial.println("Files wiped.");
    }
    else if (input.equalsIgnoreCase("append50")) {
      appendFiftyEntries(DATA_FILE, INDEX_FILE, "1234");
      Serial.println("Appended '1234' fifty times.");
    }
    else {
      Serial.println("Unknown command. Try: append <text>, recall <n>, wipe, or append50");
    }
  }
}