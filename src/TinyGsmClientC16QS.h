/**
 * @file       TinyGsmClientC16QS.h
 * @author     Ben Wheeler
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2024 Ben Wheeler
 * @date       Mar 2024
 */

#ifndef SRC_TINYGSMCLIENTC16QS_H_
#define SRC_TINYGSMCLIENTC16QS_H_
// #pragma message("TinyGSM:  TinyGsmClientC16QS")

// #define TINY_GSM_DEBUG Serial
// #define TINY_GSM_USE_HEX

#define TINY_GSM_MUX_COUNT 8
// #define TINY_GSM_BUFFER_READ_AND_CHECK_SIZE
#define TINY_GSM_NO_MODEM_BUFFER // The C16QS just dumps received data immediately out after +CIPRECEIVE: [mux],[bytes]

#include "TinyGsmBattery.tpp"
#include "TinyGsmCalling.tpp"
#include "TinyGsmGPRS.tpp"
#include "TinyGsmGSMLocation.tpp"
#include "TinyGsmModem.tpp"
#include "TinyGsmSMS.tpp"
#include "TinyGsmSSL.tpp"
#include "TinyGsmTCP.tpp"
#include "TinyGsmTime.tpp"
#include "TinyGsmNTP.tpp"

#define GSM_NL "\r\n"
static const char GSM_OK[] TINY_GSM_PROGMEM = "OK" GSM_NL;
static const char GSM_ERROR[] TINY_GSM_PROGMEM = "ERROR" GSM_NL;
#if defined TINY_GSM_DEBUG
static const char GSM_CME_ERROR[] TINY_GSM_PROGMEM = GSM_NL "+CME ERROR:";
static const char GSM_CMS_ERROR[] TINY_GSM_PROGMEM = GSM_NL "+CMS ERROR:";
#endif

enum RegStatus
{
  REG_NO_RESULT = -1,
  REG_UNREGISTERED = 0,
  REG_SEARCHING = 2,
  REG_DENIED = 3,
  REG_OK_HOME = 1,
  REG_OK_ROAMING = 5,
  REG_UNKNOWN = 4,
};
class TinyGsmC16QS : public TinyGsmModem<TinyGsmC16QS>,
                     public TinyGsmGPRS<TinyGsmC16QS>,
                     public TinyGsmTCP<TinyGsmC16QS, TINY_GSM_MUX_COUNT>,
                     public TinyGsmSSL<TinyGsmC16QS>,
                     public TinyGsmCalling<TinyGsmC16QS>,
                     public TinyGsmSMS<TinyGsmC16QS>,
                     public TinyGsmGSMLocation<TinyGsmC16QS>,
                     public TinyGsmTime<TinyGsmC16QS>,
                     public TinyGsmNTP<TinyGsmC16QS>,
                     public TinyGsmBattery<TinyGsmC16QS>
{
  friend class TinyGsmModem<TinyGsmC16QS>;
  friend class TinyGsmGPRS<TinyGsmC16QS>;
  friend class TinyGsmTCP<TinyGsmC16QS, TINY_GSM_MUX_COUNT>;
  friend class TinyGsmSSL<TinyGsmC16QS>;
  friend class TinyGsmCalling<TinyGsmC16QS>;
  friend class TinyGsmSMS<TinyGsmC16QS>;
  friend class TinyGsmGSMLocation<TinyGsmC16QS>;
  friend class TinyGsmTime<TinyGsmC16QS>;
  friend class TinyGsmNTP<TinyGsmC16QS>;
  friend class TinyGsmBattery<TinyGsmC16QS>;

private:
  // Unlike seemingly literally every other modem supported by TinyGSM, the C16QS does not allow specifying a mux number
  // to CIPSTART. Instead, it returns the mux number, which must be extracted and used for subsequent commands. Because
  // callers of TinyGSM functions expect to provide the mux number, we use an array to translate between the caller-supplied
  // mux number (the index into this array) and the actual mux number expected by the modem (the value in that array element)
  uint8_t muxReal[TINY_GSM_MUX_COUNT];

  /*
   * Inner Client
   */
public:
  class GsmClientC16QS : public GsmClient
  {
    friend class TinyGsmC16QS;

  public:
    GsmClientC16QS() {}

    explicit GsmClientC16QS(TinyGsmC16QS &modem, uint8_t muxIdx = 0)
    {
      init(&modem, muxIdx);
    }

    bool init(TinyGsmC16QS *modem, uint8_t muxIdx = 0)
    {
      this->at = modem;
      sock_available = 0;
      prev_check = 0;
      sock_connected = false;
      got_data = false;

      if (muxIdx < TINY_GSM_MUX_COUNT)
      {
        this->mux = muxIdx;
      }
      else
      {
        this->mux = (muxIdx % TINY_GSM_MUX_COUNT);
      }
      at->sockets[this->mux] = this;

      return true;
    }

  public:
    virtual int connect(const char *host, uint16_t port, int timeout_s)
    {
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      sock_connected = at->modemConnect(host, port, mux, false, timeout_s);
      return sock_connected;
    }
    TINY_GSM_CLIENT_CONNECT_OVERRIDES

    void stop(uint32_t maxWaitMs)
    {
      TINY_GSM_YIELD();
      at->sendAT(GF("+CIPCLOSE="), mux);
      sock_connected = false;
      at->waitResponse(maxWaitMs);
      rx.clear();
    }
    void stop() override
    {
      stop(15000L);
    }

    /*
     * Extended API
     */

    String remoteIP() TINY_GSM_ATTR_NOT_IMPLEMENTED;
  };

  /*
   * Inner Secure Client
   */
public:
  class GsmClientSecureC16QS : public GsmClientC16QS
  {
  public:
    GsmClientSecureC16QS() {}

    explicit GsmClientSecureC16QS(TinyGsmC16QS &modem, uint8_t mux = 0)
        : GsmClientC16QS(modem, mux) {}

  public:
    int connect(const char *host, uint16_t port, int timeout_s) override
    {
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      sock_connected = at->modemConnect(host, port, mux, true, timeout_s);
      return sock_connected;
    }
    TINY_GSM_CLIENT_CONNECT_OVERRIDES
  };

  /*
   * Constructor
   */
public:
  explicit TinyGsmC16QS(Stream &stream) : stream(stream)
  {
    memset(sockets, 0, sizeof(sockets));
    for (uint8_t i = 0; i < TINY_GSM_MUX_COUNT; i++)
    {
      muxReal[i] = -1;
    }
  }

  /*
   * Basic functions
   */
protected:
  bool initImpl(const char *pin = NULL)
  {
    DBG(GF("### TinyGSM Version:"), TINYGSM_VERSION);
    DBG(GF("### TinyGSM Compiled Module:  TinyGsmClientC16QS"));

    if (!testAT())
    {
      return false;
    }

    // sendAT(GF("&FZ"));  // Factory + Reset
    // waitResponse();

    sendAT(GF("E0")); // Echo Off
    if (waitResponse() != 1)
    {
      return false;
    }

#ifdef TINY_GSM_DEBUG
    sendAT(GF("+CMEE=2")); // turn on verbose error codes
#else
    sendAT(GF("+CMEE=0")); // turn off error codes
#endif
    waitResponse();

    DBG(GF("### Modem:"), getModemName());

    // Tell Cavli modem to use external, not internal SIM.
    // Sending this twice appears to be necessary. If not done, you get "use not powered on" error
    sendAT("^SIMSWAP=1");
    while (waitResponse(1000, "+CAVEUICCSUPPORT:") != 1)
      ;
    sendAT("^SIMSWAP=1");
    while (waitResponse(1000, "+CAVEUICCSUPPORT:") != 1)
      ;

    SimStatus ret = getSimStatus();
    // if the sim isn't ready and a pin has been provided, try to unlock the sim
    if (ret != SIM_READY && pin != NULL && strlen(pin) > 0)
    {
      simUnlock(pin);
      return (getSimStatus() == SIM_READY);
    }
    else
    {
      // if the sim is ready, or it's locked but no pin has been provided,
      // return true
      return (ret == SIM_READY || ret == SIM_LOCKED);
    }
  }

  String getModemNameImpl()
  {
    String name = "[Cavli C16QS]";

    // It may return OK before or after the +CGMM reply.
    sendAT(GF("+CGMM"));
    String res2;
    if (waitResponse(1000, "+CGMM: ") != 1)
    {
      return name;
    }
    name = stream.readStringUntil('\n');
    name.trim();
    return name;
  }

  bool factoryDefaultImpl()
  {
    sendAT(GF("&F")); // Factory
    waitResponse();
    sendAT(GF("E0")); // Echo off
    waitResponse();
    sendAT(GF("&W")); // Write configuration
    return waitResponse() == 1;
  }

  /*
   * Power functions
   */
protected:
  bool restartImpl(const char *pin = NULL)
  {
    if (!testAT())
    {
      return false;
    }
    sendAT(GF("&W"));
    waitResponse();
    if (!setPhoneFunctionality(0))
    {
      return false;
    }
    if (!setPhoneFunctionality(1, true))
    {
      return false;
    }
    delay(3000);
    return init(pin);
  }

  bool powerOffImpl()
  {
    sendAT(GF("+CFUN=0"));
    return waitResponse(10000L) == 1;
  }

  // During sleep, the C16QS module has its serial communication disabled. In
  // order to reestablish communication the module must be woken up using its reset (or maybe DTR) pins
  bool sleepEnableImpl(bool enable = true)
  {
    sendAT(GF("$QCSLEEP=2"), enable); // 1 = Hibernate, 2 = Sleep2, 3 = Sleep1, 4 = Off
    return waitResponse() == 1;
  }

  // <fun> 0 Minimum functionality
  // <fun> 1 Full functionality (Default)
  // <fun> 4 Disable phone both transmit and receive RF circuits.
  // <rst> Reset the MT before setting it to <fun> power level.
  bool setPhoneFunctionalityImpl(uint8_t fun, bool reset = false)
  {
    sendAT(GF("+CFUN="), fun, reset ? ",1" : "");
    return waitResponse(10000L) == 1;
  }

  /*
   * Generic network functions
   */
public:
  RegStatus getRegistrationStatus()
  {
    return (RegStatus)getRegistrationStatusXREG("CREG"); // Can also use CEREG but I think they return the same values
  }

  // BW: The C16QS doesn't have a direct equivalent to AT+CNSMOD?, perhaps unsurprisingly as it
  // only has one type of connection (Cat1bis). So instead we use AT+CSCON? to see if we are
  // connected, and if so return 99, which is not returned by any current SIMCom module.
  bool getNetworkSystemMode(bool &n, int16_t &stat)
  {
    // n: whether to automatically report the system mode info
    // stat: the current service. 0 if it not connected
    sendAT(GF("+CSCON?"));
    if (waitResponse(GF(GSM_NL "+CSCON:")) != 1)
    {
      return false;
    }
    n = streamGetIntBefore(',') != 0;
    stat = streamGetIntBefore('\n');
    if (stat == 1)
    {
      stat = 99;
    }
    waitResponse();
    return true;
  }

protected:
  bool isNetworkConnectedImpl()
  {
    RegStatus s = getRegistrationStatus();
    return (s == REG_OK_HOME || s == REG_OK_ROAMING);
  }

  String getLocalIPImpl()
  {
    sendAT(GF("+CGPADDR=1"));
    // Result should be something like:
    // 1,10.20.30.40
    String res;
    if (waitResponse(10000L, res) != 1)
    {
      return "";
    }
    res.replace(GSM_NL "OK" GSM_NL, "");
    res.replace(GSM_NL, "");
    res.replace("1,", "");
    res.trim();
    return res;
  }

  /*
   * GPRS functions
   */
protected:
  bool gprsConnectImpl(const char *apn, const char *user = NULL,
                       const char *pwd = NULL)
  {
    gprsDisconnect();

    // Attach to GPRS
    sendAT(GF("+CGATT=1"));
    if (waitResponse(60000L) != 1)
    {
      return false;
    }

    // Bearer settings for applications based on IP
    // Set username and password
    sendAT(GF("+CGAUTH=1,0,\""), user, "\",\"", pwd, "\"");
    waitResponse();

    // Define the PDP context
    sendAT(GF("+CGDCONT=1,\"IP\",\""), apn, '"');
    waitResponse();

    // Activate the PDP context
    sendAT(GF("+CGACT=1,1"));
    waitResponse(60000L);

    // Set to multi-IP
    sendAT(GF("+CIPMUX=1"));
    if (waitResponse() != 1)
    {
      return false;
    }

    // Set TCP Mode to "raw" otherwise null bytes are dropped
    sendAT(GF("+TCPFMT=2"));
    if (waitResponse() != 1)
    {
      return false;
    }

    // // Put in "quick send" mode (thus no extra "Send OK")
    // sendAT(GF("+CIPQSEND=1"));
    // if (waitResponse() != 1)
    // {
    //   return false;
    // }

    // Configure Domain Name Server (DNS)
    sendAT(GF("+CDNSCFG=\"8.8.8.8\",\"8.8.4.4\""));
    if (waitResponse() != 1)
    {
      return false;
    }

    return true;
  }

  bool gprsDisconnectImpl()
  {
    // Shut the TCP/IP connection
    // CIPSHUT will close *all* open connections
    sendAT(GF("+CIPSHUT"));
    if (waitResponse(60000L) != 1)
    {
      return false;
    }

    sendAT(GF("+CGATT=0")); // Detach from GPRS
    if (waitResponse(60000L) != 1)
    {
      return false;
    }

    return true;
  }

  /*
   * SIM card functions
   */
protected:
  // May not return the "+ICCID" before the number
  String getSimCCIDImpl()
  {
    sendAT(GF("+ICCID"));
    if (waitResponse(GF(GSM_NL)) != 1)
    {
      return "";
    }
    String res = stream.readStringUntil('\n');
    waitResponse();
    // Trim out the CCID header in case it is there
    res.replace("+ICCID:", "");
    res.trim();
    return res;
  }

  /*
   * Phone Call functions
   */
public:
  /*
   * Messaging functions
   */
protected:
  // Follows all messaging functions per template

  /*
   * GSM Location functions
   */
protected:
  // Depending on the exacty model and firmware revision, should return a
  // GSM-based location from CLBS as per the template
  // TODO(?):  Check number of digits in year (2 or 4)

  /*
   * GPS/GNSS/GLONASS location functions
   */
protected:
  // No functions of this type supported

  /*
   * Time functions
   */
protected:
  // Can follow the standard CCLK function in the template

  /*
   * NTP server functions
   */
  // Can sync with server using CNTP as per template

  /*
   * Battery functions
   */
protected:
  // Follows all battery functions per template

  /*
   * NTP server functions
   */
  // Can sync with server using CNTP as per template

  /*
   * Client related functions
   */
protected:
  // TinyGSM expects to supply the mux number to CIPSTART, but the C16QS doesn't allow this.
  // We instead set the object's mux attribute according to the response from CIPSTART.
  // this->muxReal array is used to translate between caller-supplied mux and the real mux number used by the modem.
  bool modemConnect(const char *host, uint16_t port, uint8_t muxIdx,
                    bool ssl = false, int timeout_s = 75)
  {
    int8_t rsp;
    String res;
    uint32_t timeout_ms = ((uint32_t)timeout_s) * 1000;
    sendAT(GF("+CIPSTART="), GF("\"TCP"), GF("\",\""), host,
           GF("\","), port, GF(","), (ssl ? "1" : "0"));
    if (waitResponse(timeout_ms, res, GF("CONNECT OK" GSM_NL), GF("CONNECT FAIL" GSM_NL), GF("ALREADY CONNECT" GSM_NL), GF("ERROR" GSM_NL), GF("CLOSE OK" GSM_NL)) != 1)
    {
      return false;
    }
    // res will now be something like "+CIPSTART: 0,CONNECT OK", out of which we need the 0
    res.replace(GSM_NL "OK" GSM_NL, "");
    res.replace(GSM_NL, "");
    res.replace(",CONNECT OK", "");
    res.replace("+CIPSTART: ", "");
    uint8_t mux = res.toInt();
    muxReal[muxIdx] = mux;
    DBG("Modem mux", mux, "mapped to requested mux index", muxIdx);
    waitResponse(); // OK
    return true;
  }

  int16_t modemSend(const uint8_t *buff, size_t len, uint8_t muxIdx)
  {
    DBG("Starting send");
    sendAT(GF("+CIPSEND="), muxReal[muxIdx], ',', (uint16_t)len);
    if (waitResponse(GF(">")) != 1)
    {
      return 0;
    }
    Serial.print("Sending: {");
    for (int i = 0; i < len; i++)
    {
      Serial.printf("0x%02x,", buff[i]);
    }
    Serial.println("};");
    stream.write(buff, len);
    stream.flush();
    if (waitResponse(GF("SEND OK")) != 1)
    {
      Serial.println("SEND FAILED!?");
      return 0;
    }
    // The number of bytes sent is not returned by the C16QS, so we just have to assume we sent them all
    return len;
  }

  bool modemGetConnected(uint8_t muxIdx)
  {
    return sockets[muxIdx]->sock_connected;
    // uint8_t mux = muxReal[muxIdx];
    // sendAT(GF("+CIPSTATUS="), mux);
    // // For some reason on the C16QS replies to AT+CIPSTATUS=[mux] start with C: rather than +CIPSTATUS:
    // // Lines look like
    // // C:0,TCP,141.147.75.82,1883,CONNECTED
    // if (waitResponse(GF("C:")) != 1)
    //   return false;
    // // streamSkipUntil(','); // skip mux
    // // streamSkipUntil(','); // skip TCP/UDP
    // // streamSkipUntil(','); // skip IP address
    // // streamSkipUntil(','); // skip port
    // // So unlike SimCom, the status does not have quotes around it
    // int8_t res = waitResponse(GF(",CONNECTED"), GF(",CLOSED"),
    //                           GF(",CLOSING"), GF(",REMOTE CLOSING"),
    //                           GF(",INITIAL"));
    // waitResponse(); // OK
    // if (sockets[muxIdx])
    // {
    //   sockets[muxIdx]->sock_connected = true;
    // }
    // if (1 == res)
    // {
    //   Serial.printf("muxIdx %u is connected", muxIdx);
    //   return true;
    // }
    // return false;
  }

  /*
   * Utilities
   */
public:
  int8_t realMuxToIdx(int8_t mux)
  {
    for (uint8_t i = 0; i < TINY_GSM_MUX_COUNT; i++)
    {
      if (muxReal[i] == mux)
      {
        return i;
      }
    }
    return -1;
  }

  // TODO(vshymanskyy): Optimize this!
  int8_t waitResponse(uint32_t timeout_ms, String &data,
                      GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL)
  {
    /*String r1s(r1); r1s.trim();
    String r2s(r2); r2s.trim();
    String r3s(r3); r3s.trim();
    String r4s(r4); r4s.trim();
    String r5s(r5); r5s.trim();
    DBG("### ..:", r1s, ",", r2s, ",", r3s, ",", r4s, ",", r5s);*/
    data.reserve(64);
    uint8_t index = 0;
    uint32_t startMillis = millis();
    do
    {
      TINY_GSM_YIELD();
      while (stream.available() > 0)
      {
        TINY_GSM_YIELD();
        int8_t a = stream.read();
        if (a <= 0)
          continue; // Skip 0x00 bytes, just in case
        data += static_cast<char>(a);
        if (r1 && data.endsWith(r1))
        {
          index = 1;
          goto finish;
        }
        else if (r2 && data.endsWith(r2))
        {
          index = 2;
          goto finish;
        }
        else if (r3 && data.endsWith(r3))
        {
#if defined TINY_GSM_DEBUG
          if (r3 == GFP(GSM_CME_ERROR))
          {
            streamSkipUntil('\n'); // Read out the error
          }
#endif
          index = 3;
          goto finish;
        }
        else if (r4 && data.endsWith(r4))
        {
          index = 4;
          goto finish;
        }
        else if (r5 && data.endsWith(r5))
        {
          index = 5;
          goto finish;
        }
        else if (data.endsWith(GF(GSM_NL "+CIPRECEIVE:")))
        {
          // Unlike SIMCom modems, the C16QS doesn't tell you that data is available and wait
          // for you to ask for it, it tells you data is available and then immediately gives
          // you the data... This code is based on ESP8266 which works similarly.
          int8_t mux = streamGetIntBefore(',');
          int16_t len = streamGetIntBefore('\n');
          // The actual data starts after another newline
          DBG("###", len, "bytes data ready to receive on real mux ", mux);
          int16_t len_orig = len;
          // But we need the mux index to look at the right sockets[] element
          int8_t muxIdx = realMuxToIdx(mux);
          DBG("### I think the socket is on muxIdx", muxIdx);
          if (muxIdx >= 0 && muxIdx < TINY_GSM_MUX_COUNT && sockets[muxIdx])
          {
            if (len > sockets[muxIdx]->rx.free())
            {
              DBG("### Buffer overflow: ", len, "received vs",
                  sockets[muxIdx]->rx.free(), "available");
            }
            else
            {
              DBG("### Got Data:", len, "bytes on mux", mux);
            }
            while (len--)
            {
              // char c = stream.peek();
              moveCharFromStreamToFifo(muxIdx);
              // char out[50];
              // snprintf(out, 49, "0x%02x", c);
              // DBG(out);
            }
            // The data is followed by CRLF which must be discarded
            streamSkipUntil('\n');

            // TODO(SRGDamia1): deal with buffer overflow/missed characters
            if (len_orig > sockets[muxIdx]->available())
            {
              DBG("### Fewer characters received than expected: ",
                  sockets[muxIdx]->available(), " vs ", len_orig);
            }
          }
          else
          {
            DBG("### Could not find muxIdx for that mux number");
          }
          data = "";
        }
        else if (data.endsWith(GF("CLOSED" GSM_NL)))
        {
          int8_t nl = data.lastIndexOf(GSM_NL, data.length() - 8);
          int8_t coma = data.indexOf(',', nl + 2);
          int8_t mux = data.substring(nl + 2, coma).toInt();
          int8_t muxIdx = realMuxToIdx(mux);
          if (muxIdx >= 0 && muxIdx < TINY_GSM_MUX_COUNT && sockets[muxIdx])
          {
            sockets[muxIdx]->sock_connected = false;
          }
          data = "";
          DBG("### Closed: ", mux);
        }
        else if (data.endsWith(GF("*PSNWID:")))
        {
          streamSkipUntil('\n'); // Refresh network name by network
          data = "";
          DBG("### Network name updated.");
        }
        else if (data.endsWith(GF("*PSUTTZ:")))
        {
          streamSkipUntil('\n'); // Refresh time and time zone by network
          data = "";
          DBG("### Network time and time zone updated.");
        }
        else if (data.endsWith(GF("+CTZV:")))
        {
          streamSkipUntil('\n'); // Refresh network time zone by network
          data = "";
          DBG("### Network time zone updated.");
        }
        else if (data.endsWith(GF("DST:")))
        {
          streamSkipUntil(
              '\n'); // Refresh Network Daylight Saving Time by network
          data = "";
          DBG("### Daylight savings time state updated.");
        }
      }
    } while (millis() - startMillis < timeout_ms);
  finish:
    if (!index)
    {
      DBG("Didn't find", r1);
      data.trim();
      if (data.length())
      {
        DBG("### Unhandled:", data);
      }
      data = "";
      return 0;
    }
    data.replace(GSM_NL, "/");
    DBG('<', index, '>', data);
    return index;
  }

  int8_t waitResponse(uint32_t timeout_ms, GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL)
  {
    String data;
    return waitResponse(timeout_ms, data, r1, r2, r3, r4, r5);
  }

  int8_t waitResponse(GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL)
  {
    return waitResponse(1000, r1, r2, r3, r4, r5);
  }

public:
  Stream &stream;

protected:
  GsmClientC16QS *sockets[TINY_GSM_MUX_COUNT];
  const char *gsmNL = GSM_NL;
};

#endif // SRC_TINYGSMCLIENTC16QS_H_
