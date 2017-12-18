/* 
 *  https://github.com/maggie-han/who-is-home
*/

#include <SPI.h>
#include <MFRC522.h>       // RFID
#include <SdFat.h>         // SDFat Library
#include <SdFatUtil.h>     // SDFat Util Library
#include <SFEMP3Shield.h>  // Mp3 Shield Library

#define RFID_SELECT_1 4
#define RFID_RESET_1 3
#define LOCK_PIN 5


#define ID_BLOCK 8
#define AUTH_BLOCK 11

const byte keyCode[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; //Keycode, made constant for all 5 RFIDs to enable the reading of the RFIDs
const String roommates[5] = {"Gertrude", "Polonius", "Laertes", "Hamlet", "Ophelia"};
const int numberOfRoommates = 5;

SdFat sd;
SFEMP3Shield player;

MFRC522 RFID = MFRC522(RFID_SELECT_1, RFID_RESET_1);
MFRC522::MIFARE_Key key;

void setup()
{
  pinMode(LOCK_PIN, OUTPUT);
  digitalWrite(LOCK_PIN, HIGH);           //high for locking

  SPI.begin();
  sd.begin(SD_SEL, SPI_HALF_SPEED);
  sd.chdir("/");

  player.begin();
  player.setVolume(0, 0);               //0 is max, 100 is very quiet
  player.setMonoMode(1);

  RFID.PCD_Init();

  for (int keyNum = 0; keyNum < 6; keyNum++)
  {
    key.keyByte[keyNum] = keyCode[keyNum];
  }
  Serial.begin(9600);
}

bool reloadRFID()
{
  RFID.PCD_StopCrypto1();
  //RFIDs[RFIDNum].PICC_ReadCardSerial(); // re-include for slower reloads but possible better working reloads
  RFID.PICC_IsNewCardPresent();
  if (!RFID.PICC_ReadCardSerial()) // so that if it can't read the card it returns
  {
    return false;
  }
  return true;
}

bool authRFID(int block)
{
  MFRC522::StatusCode status = (MFRC522::StatusCode)RFID.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(RFID.uid)); //authenticates to gain access to sector 1, uid is like the reader's peraonl ID and can be changed but I didn't bother doing that
  if (status != MFRC522::STATUS_OK)
  {
    return false;
  }
  return true;
}

bool readRFID(byte block, byte* someBuffer)
{
  byte sizeOfBuffer = 18;
  MFRC522::StatusCode status = (MFRC522::StatusCode)RFID.MIFARE_Read(block, someBuffer, &sizeOfBuffer); //reads the data (from a block in a sector you've authenticated with), note the pointer to size instead of just the size, I don't know why they did it this way

  if (status != MFRC522::STATUS_OK)
  {
    return false;
  }

  return true;
}

byte getBlockState(int block) //255 = error, 0 = no consistancy
{
  byte someBuffer[18];
  if (readRFID(block, someBuffer) == false)
  {
    return 255;
  }

  for (int bufNum = 1; bufNum < 16; bufNum ++)
  {
    if (someBuffer[bufNum] != someBuffer[bufNum - 1])
    {
      return 0;
    }
  }
  return someBuffer[0];
}

void unlockDoor()
{
  digitalWrite(LOCK_PIN, LOW);
  delay(10000);
  digitalWrite(LOCK_PIN, HIGH);
}

bool isValidCardPresent()
{
  byte blockState = getBlockState(ID_BLOCK);
  if (blockState > 0 && blockState < 5)
  {
    return true;
  }
  return false;
}

String whoIsHome(int ID_num) {
  if (ID_num > 0 && ID_num < numberOfRoommates)
    return roommates[ID_num - 1];
  return "nobody";
}

void generateBlock (byte repeatedByte, byte* returnArray)
{
  for (int byteNum = 0; byteNum < 16; byteNum++)
  {
    returnArray[byteNum] = repeatedByte;
  }
}

void loop()
{
  player.pauseDataStream();
  if (reloadRFID())
  {
    if (authRFID(AUTH_BLOCK) && isValidCardPresent())
    {
      int ID = getBlockState(ID_BLOCK);
      String person = whoIsHome(ID);
      if (! person.equals("nobody")){
        Serial.println(person+" is home!");
        player.playTrack(ID);
        unlockDoor();
      }
    }
  }
  player.resumeDataStream();
  delay(100);
}
