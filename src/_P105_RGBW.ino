#include "_Plugin_Helper.h"
#ifdef USES_P105

// #######################################################################################################
// #################################### Plugin 105: RGBW / Milight #######################################
// #######################################################################################################

# define PLUGIN_105
# define PLUGIN_ID_105         105
# define PLUGIN_NAME_105       "RGBW MiLight"

# include <Ticker.h>

boolean Plugin_105_init = false;
WiFiUDP Plugin_105_milightUDP;
int     Plugin_105_FadingRate        = 1; //   was 10hz
unsigned int Plugin_105_UDPCmd       = 0;
unsigned int Plugin_105_UDPParameter = 0;
Ticker Plugin_105_Ticker;


struct Plugin_105_structPins
{
  unsigned long FadingTimer          = 0;
  int           CurrentLevel         = 0;
  int           FadingTargetLevel    = 0;
  int           FadingMMillisPerStep = 0;
  int           FadingDirection      = 0;
  int           PinNo                = 0;
} Plugin_105_Pins[4];

struct Plugin_105_structRGBFlasher
{
  unsigned int Count = 0;
  unsigned int OnOff = 0;
  unsigned int Freq  = 0;
  unsigned int Red   = 0;
  unsigned int Green = 0;
  unsigned int Blue  = 0;
} Plugin_105_RGBFlasher;

struct Plugin_105_structMiLight
{
  float        HueLevel = 0;
  float        LumLevel = 0.5;
  float        SatLevel = 1;
  boolean      ColourOn = false;
  boolean      WhiteOn  = false;
  unsigned int UDPPort  = 0;
} Plugin_105_MiLight;


/************************/
/* handle fading timer */
/***********************/
void Plugin_105_FadingTimer()
{
  // Fading
  for (int PinIndex = 0; PinIndex < 4; PinIndex++)
  {
    if (Plugin_105_Pins[PinIndex].FadingDirection != 0)
    {
      if (millis() > Plugin_105_Pins[PinIndex].FadingTimer)
      {
        Plugin_105_Pins[PinIndex].FadingTimer  = millis() + Plugin_105_Pins[PinIndex].FadingMMillisPerStep;
        Plugin_105_Pins[PinIndex].CurrentLevel = Plugin_105_Pins[PinIndex].CurrentLevel + Plugin_105_Pins[PinIndex].FadingDirection;

        if ((Plugin_105_Pins[PinIndex].CurrentLevel >= Plugin_105_Pins[PinIndex].FadingTargetLevel) &&
            (Plugin_105_Pins[PinIndex].FadingDirection > 0))
        {
          Plugin_105_Pins[PinIndex].FadingDirection = 0;
          Plugin_105_Pins[PinIndex].CurrentLevel    = Plugin_105_Pins[PinIndex].FadingTargetLevel;
          addLog(LOG_LEVEL_INFO, F("Fade up complete"));
        }

        if ((Plugin_105_Pins[PinIndex].CurrentLevel <= Plugin_105_Pins[PinIndex].FadingTargetLevel) &&
            (Plugin_105_Pins[PinIndex].FadingDirection < 0))
        {
          Plugin_105_Pins[PinIndex].FadingDirection = 0;
          Plugin_105_Pins[PinIndex].CurrentLevel    = Plugin_105_Pins[PinIndex].FadingTargetLevel;
          addLog(LOG_LEVEL_INFO, F("Fade down complete"));
        }
        analogWrite(Plugin_105_Pins[PinIndex].PinNo, Plugin_105_Pins[PinIndex].CurrentLevel);
      }
    }
  }
}

/**********************************************************************/
/* handle udp packets for milight emulation*/
/**********************************************************************/
void Plugin_105_ProcessUDP()
{
  boolean MiLightUpdate = false;

  switch (int(Plugin_105_UDPCmd))
  {
    case 65: // off
    case 70:
    case 33:
    {
      Plugin_105_MiLight.ColourOn = false;
      MiLightUpdate               = true;
      break;
    }
    case 66: // on
    case 69:
    case 74:
    {
      Plugin_105_MiLight.ColourOn = true;
      MiLightUpdate               = true;
      break;
    }
    case 32: // set colour
    case 64:
    {
      Plugin_105_MiLight.HueLevel = 256 - Plugin_105_UDPParameter + 192;
      Plugin_105_MiLight.HueLevel = (Plugin_105_MiLight.HueLevel / 256) * 360;
      Plugin_105_MiLight.HueLevel = int(Plugin_105_MiLight.HueLevel) % 360;
      Plugin_105_MiLight.HueLevel = Plugin_105_MiLight.HueLevel / 360;
      MiLightUpdate               = true;
      Plugin_105_MiLight.ColourOn = true;
      break;
    }
    case 78: // brightness
    {
      Plugin_105_MiLight.LumLevel = Plugin_105_UDPParameter;
      Plugin_105_MiLight.LumLevel = Plugin_105_MiLight.LumLevel / 54;
      MiLightUpdate               = true;
      Plugin_105_MiLight.ColourOn = true;
      break;
    }
    case 35: // increase brightness
    {
      Plugin_105_MiLight.LumLevel = Plugin_105_MiLight.LumLevel + 0.05;

      if (Plugin_105_MiLight.LumLevel > 1) { Plugin_105_MiLight.LumLevel = 1; }
      MiLightUpdate               = true;
      Plugin_105_MiLight.ColourOn = true;
      break;
    }
    case 36: // decrease brightness
    {
      Plugin_105_MiLight.LumLevel = Plugin_105_MiLight.LumLevel - 0.05;

      if (Plugin_105_MiLight.LumLevel < 0) { Plugin_105_MiLight.LumLevel = 0; }
      MiLightUpdate               = true;
      Plugin_105_MiLight.ColourOn = true;
      break;
    }
    case 39: // increase saturation
    {
      Plugin_105_MiLight.SatLevel = Plugin_105_MiLight.SatLevel + 0.05;

      if (Plugin_105_MiLight.SatLevel > 1) { Plugin_105_MiLight.SatLevel = 1; }
      MiLightUpdate               = true;
      Plugin_105_MiLight.ColourOn = true;
      break;
    }
    case 40: // decrease saturation
    {
      Plugin_105_MiLight.SatLevel = Plugin_105_MiLight.SatLevel - 0.05;

      if (Plugin_105_MiLight.SatLevel < 0) { Plugin_105_MiLight.SatLevel = 0; }
      MiLightUpdate               = true;
      Plugin_105_MiLight.ColourOn = true;
      break;
    }
  }

  if (MiLightUpdate == true)
  {
    if (Plugin_105_MiLight.ColourOn == true)
    {
      // Plugin_105_HSL2Rgb(Plugin_105_MiLight.HueLevel, Plugin_105_MiLight.SatLevel, Plugin_105_MiLight.LumLevel);

      if (Plugin_105_RGBFlasher.Count == 0) // only change led colour if not flashing, selected colour will be applied after flashing
                                            // concludes
      {
        analogWrite(Plugin_105_Pins[0].PinNo, Plugin_105_Pins[0].CurrentLevel);
        analogWrite(Plugin_105_Pins[1].PinNo, Plugin_105_Pins[1].CurrentLevel);
        analogWrite(Plugin_105_Pins[2].PinNo, Plugin_105_Pins[2].CurrentLevel);

        // Serial.println("Setting RGB To:");
        // Serial.println(Plugin_105_Pins[0].CurrentLevel);
        // Serial.println(Plugin_105_Pins[1].CurrentLevel);
        // Serial.println(Plugin_105_Pins[2].CurrentLevel);
      }
    }
    else
    {
      if (Plugin_105_RGBFlasher.Count == 0) // only change led colour if not flashing, selected colour will be applied after flashing
                                            // concludes
      {
        analogWrite(Plugin_105_Pins[0].PinNo, 0);
        analogWrite(Plugin_105_Pins[1].PinNo, 0);
        analogWrite(Plugin_105_Pins[2].PinNo, 0);
      }
    }
  }
}

float Plugin_105_Hue2RGB(float v1, float v2, float vH)
{
  if (vH < 0) { vH += 1; }

  if (vH > 1) { vH -= 1; }

  if ((6 * vH) < 1) { return v1 + (v2 - v1) * 6 * vH; }

  if ((2 * vH) < 1) { return v2; }

  if ((3 * vH) < 2) { return v1 + (v2 - v1) * ((0.66666666666) - vH) * 6; }
  return v1;
}

void Plugin_105_HSL2Rgb(float h, float s, float l)
{
  float holdval;
  float r = 0;
  float g = 0;
  float b = 0;

  if (s == 0)
  {
    r = 1;
    g = 1;
    b = 1;
  }
  else
  {
    float q;

    if (l < 0.5)
    {
      q = l * (1 + s);
    }
    else
    {
      q = l + s - (l * s);
    }

    float p = (2 * l) - q;

    holdval = h + 0.3333333333f;
    r       = Plugin_105_Hue2RGB(p, q, holdval);

    holdval = h;
    g       = Plugin_105_Hue2RGB(p, q, holdval);

    holdval = h - 0.3333333333f;
    b       = Plugin_105_Hue2RGB(p, q, holdval);
  }

  r = r * 1023 + 0.5f;
  g = g * 1023 + 0.5f;
  b = b * 1023 + 0.5f;

  Plugin_105_Pins[0].CurrentLevel = floor(r);
  Plugin_105_Pins[1].CurrentLevel = floor(g);
  Plugin_105_Pins[2].CurrentLevel = floor(b);

  // fail safes
  if (Plugin_105_Pins[0].CurrentLevel > 1023) { Plugin_105_Pins[0].CurrentLevel = 1023; }

  if (Plugin_105_Pins[1].CurrentLevel > 1023) { Plugin_105_Pins[1].CurrentLevel = 1023; }

  if (Plugin_105_Pins[2].CurrentLevel > 1023) { Plugin_105_Pins[2].CurrentLevel = 1023; }
}

boolean Plugin_105(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
    {
      Device[++deviceCount].Number = PLUGIN_ID_105;
      Device[deviceCount].Type     = DEVICE_TYPE_SINGLE;
      Device[deviceCount].Custom   = true;
      Device[deviceCount].VType    = Sensor_VType::SENSOR_TYPE_DIMMER;
      Device[deviceCount].Ports    = 0;
      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      string = F(PLUGIN_NAME_105);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
      break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {
      addFormSubHeader(F("RGBW Settings"));
      // Limited to valid ports, should default to 8899, but not implemented.
      addFormNumericBox(F("Milight UDP Port"), F("plugin_105_port"),     ExtraTaskSettings.TaskDevicePluginConfigLong[0], 1, 65535); 
      // Limited to available GPIO pins only
      addFormNumericBox(F("Red Pin"),          F("plugin_105_RedPin"),   ExtraTaskSettings.TaskDevicePluginConfigLong[1], 0, 16);
      addFormNumericBox(F("Green Pin"),        F("plugin_105_GreenPin"), ExtraTaskSettings.TaskDevicePluginConfigLong[2], 0, 16);
      addFormNumericBox(F("Blue Pin"),         F("plugin_105_BluePin"),  ExtraTaskSettings.TaskDevicePluginConfigLong[3], 0, 16);
      addFormNumericBox(F("White Pin"),        F("plugin_105_WhitePin"), ExtraTaskSettings.TaskDevicePluginConfigLong[4], 0, 16);

      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
      String plugin1 = web_server.arg(F("plugin_105_port"));
      ExtraTaskSettings.TaskDevicePluginConfigLong[0] = plugin1.toInt();
      String plugin2 = web_server.arg(F("plugin_105_RedPin"));
      ExtraTaskSettings.TaskDevicePluginConfigLong[1] = plugin2.toInt();
      String plugin3 = web_server.arg(F("plugin_105_GreenPin"));
      ExtraTaskSettings.TaskDevicePluginConfigLong[2] = plugin3.toInt();
      String plugin4 = web_server.arg(F("plugin_105_BluePin"));
      ExtraTaskSettings.TaskDevicePluginConfigLong[3] = plugin4.toInt();
      String plugin5 = web_server.arg(F("plugin_105_WhitePin"));
      ExtraTaskSettings.TaskDevicePluginConfigLong[4] = plugin5.toInt();
      SaveTaskSettings(event->TaskIndex);
      success = true;
      break;
    }

    case PLUGIN_INIT:
    {
      LoadTaskSettings(event->TaskIndex);

      // udp port
      Plugin_105_MiLight.UDPPort = ExtraTaskSettings.TaskDevicePluginConfigLong[0];

      if (Plugin_105_MiLight.UDPPort != 0)
      {
        if (Plugin_105_milightUDP.begin(Plugin_105_MiLight.UDPPort)) { addLog(LOG_LEVEL_INFO, F("INIT: Milight UDP")); }
      }

      // rgbw gpio pins
      boolean SetupTimer = false;

      for (int PinIndex = 0; PinIndex < 4; PinIndex++)
      {
        Plugin_105_Pins[PinIndex].PinNo = ExtraTaskSettings.TaskDevicePluginConfigLong[PinIndex + 1];

        if (Plugin_105_Pins[PinIndex].PinNo != 0)
        {
          pinMode(Plugin_105_Pins[PinIndex].PinNo, OUTPUT);
          digitalWrite(Plugin_105_Pins[PinIndex].PinNo, LOW);
          SetupTimer = true;
        }
      }

      if (SetupTimer == true)
      {
        addLog(LOG_LEVEL_INFO, F("INIT: Milight Fading Timer"));
        Plugin_105_Ticker.attach_ms(20, Plugin_105_FadingTimer);
      }

      Plugin_105_init = true;
      success         = true;
      break;
    }

    case PLUGIN_TEN_PER_SECOND:
    {
      if (Plugin_105_init)
      {
        // UDP events for milight emulation
        if (Plugin_105_MiLight.UDPPort != 0)
        {
          int packetSize = Plugin_105_milightUDP.parsePacket();

          if (packetSize)
          {
            char packetBuffer[128];
            int  len = Plugin_105_milightUDP.read(packetBuffer, 128);

            if ((len == 3) && (packetBuffer[2] == 85))
            {
              // Serial.println("Commands received ");
              // Serial.println(int(packetBuffer[0]));
              // Serial.println(int(packetBuffer[1]));
              // Serial.println(int(packetBuffer[2]));
              Plugin_105_UDPCmd       = packetBuffer[0];
              Plugin_105_UDPParameter = packetBuffer[1];
              Plugin_105_ProcessUDP();
            }
          }
        }

        // RGB flashing
        if ((Plugin_105_RGBFlasher.Count > 0) && (millis() > Plugin_105_RGBFlasher.Freq))
        {
          Plugin_105_RGBFlasher.Freq  = millis() + 500; // half second flash rate
          Plugin_105_RGBFlasher.OnOff = 1 - Plugin_105_RGBFlasher.OnOff;

          if (Plugin_105_RGBFlasher.OnOff == 1)
          {
            analogWrite(Plugin_105_Pins[0].PinNo, Plugin_105_RGBFlasher.Red);
            analogWrite(Plugin_105_Pins[1].PinNo, Plugin_105_RGBFlasher.Green);
            analogWrite(Plugin_105_Pins[2].PinNo, Plugin_105_RGBFlasher.Blue);
          }
          else
          {
            analogWrite(Plugin_105_Pins[0].PinNo, 0);
            analogWrite(Plugin_105_Pins[1].PinNo, 0);
            analogWrite(Plugin_105_Pins[2].PinNo, 0);
            Plugin_105_RGBFlasher.Count = Plugin_105_RGBFlasher.Count - 1;
          }

          if (Plugin_105_RGBFlasher.Count == 0)
          {
            if (Plugin_105_MiLight.ColourOn == true)
            {
              addLog(LOG_LEVEL_INFO, F("Restoring to colour..."));
              analogWrite(Plugin_105_Pins[0].PinNo, Plugin_105_Pins[0].CurrentLevel);
              analogWrite(Plugin_105_Pins[1].PinNo, Plugin_105_Pins[1].CurrentLevel);
              analogWrite(Plugin_105_Pins[2].PinNo, Plugin_105_Pins[2].CurrentLevel);
            }
            addLog(LOG_LEVEL_INFO, F("Flashing RGB complete"));
          }
        }

        // Fading - moved to timer section
      }

      success = true;
      break;
    }

    case PLUGIN_WRITE:
    {
      const String command = parseString(string, 1);
      int Par[8] = { 0 };

      {
        String TmpStr1;
        for (int i = 1; i < 8; ++i) {
            if (GetArgv(string.c_str(), TmpStr1, i+1)) { 
            Par[i] = TmpStr1.toInt(); 
            }
        }
      }

      // initialise LED Flashing if not flashing already
      if (command.equalsIgnoreCase(F("RGBFLASH")) && (Plugin_105_RGBFlasher.Count == 0) && (Par[1] <= 1023) && (Par[2] <= 1023) &&
          (Par[3] <= 1023) && (Par[4] > 0) && (Par[4] <= 20))
      {
        success                     = true;
        Plugin_105_RGBFlasher.Red   = Par[1];
        Plugin_105_RGBFlasher.Green = Par[2];
        Plugin_105_RGBFlasher.Blue  = Par[3];
        Plugin_105_RGBFlasher.Count = Par[4];
        Plugin_105_RGBFlasher.OnOff = 0;
        Plugin_105_RGBFlasher.Freq  = millis() + 500;

        // conclude any ongoing rgb fades
        for (int PinIndex = 0; PinIndex < 3; PinIndex++)
        {
          if (Plugin_105_Pins[PinIndex].FadingDirection != 0)
          {
            Plugin_105_Pins[PinIndex].FadingDirection = 0;
            Plugin_105_Pins[PinIndex].CurrentLevel    = Plugin_105_Pins[PinIndex].FadingTargetLevel;
          }
        }

        if (printToWeb)
        {
          printWebString += F("RGB flashing for ");
          printWebString += Par[4];
          printWebString += F(" times.");
          printWebString += F("<BR>");
        }
        addLog(LOG_LEVEL_INFO, F("Start PWM Flash"));
      }

      // initialise LED Fading pin 0=r,1=g,2=b,3=w
      if (command.equalsIgnoreCase(F("PWMFADE")))
      {
        success = true;

        if ((Par[2] >= 0) && (Par[2] <= 1023) && (Par[1] >= 0) && (Par[1] <= 3) && (Par[3] > 0) && (Par[3] < 30))
        {
          if ((Par[1] == 3) || ((Plugin_105_RGBFlasher.Count == 0) && (Plugin_105_RGBFlasher.OnOff == 0))) // white pin or no flashing going
                                                                                                           // so init fade
          {
            Plugin_105_Pins[Par[1]].FadingTargetLevel    = Par[2];
            Plugin_105_Pins[Par[1]].FadingMMillisPerStep = 1000 / Plugin_105_FadingRate;
            Plugin_105_Pins[Par[1]].FadingDirection      =
              (abs(Plugin_105_Pins[Par[1]].FadingTargetLevel - Plugin_105_Pins[Par[1]].CurrentLevel)) / (Plugin_105_FadingRate * Par[3]);

            if (Plugin_105_Pins[Par[1]].FadingDirection == 0) { Plugin_105_Pins[Par[1]].FadingDirection = 1; }

            if (Plugin_105_Pins[Par[1]].CurrentLevel ==
                Plugin_105_Pins[Par[1]].FadingTargetLevel) { Plugin_105_Pins[Par[1]].FadingDirection = 0; }

            if (Plugin_105_Pins[Par[1]].CurrentLevel >
                Plugin_105_Pins[Par[1]].FadingTargetLevel) { Plugin_105_Pins[Par[1]].FadingDirection =
                                                               Plugin_105_Pins[Par[1]].FadingDirection *
                                                               -1; }
            Plugin_105_Pins[Par[1]].FadingTimer = millis();

            if (printToWeb)
            {
              printWebString += F("PWM fading over ");
              printWebString += Par[3];
              printWebString += F(" seconds.");
              printWebString += F("<BR>");
            }
            addLog(LOG_LEVEL_INFO, F("Start PWM Fade"));
          }
          else // currently flashing so set fade completed
          {
            Plugin_105_Pins[Par[1]].FadingTargetLevel = Par[2];
            Plugin_105_Pins[Par[1]].FadingDirection   = 0;
            Plugin_105_Pins[Par[1]].CurrentLevel      = Plugin_105_Pins[Par[1]].FadingTargetLevel;
          }
        }
      }

      break;
    }
  }

  return success;
}

#endif // ifdef USES_P105
