#pragma once
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>
#include <functional>
#include "ContentLogic.h"
#include "PassportProtocol.h"
// Explicitly paired LAN receiver. A one-time challenge binds each signed write.
class PassportLink {
 public:
  using Save = std::function<bool(const String&,uint32_t,bool)>;
  String key;
  void begin(Save save) {
    save_ = save; prefs_.begin("passport-link",false);key=prefs_.getString("key","");
    server_.on("/v1/challenge",HTTP_GET,[this](){
      if(key.length()!=32){server_.send(403,"text/plain","Pair first");return;}
      char value[33];for(int i=0;i<4;i++)snprintf(value+i*8,9,"%08lx",(unsigned long)esp_random());
      challenges_.issue(value,millis());server_.sendHeader("Cache-Control","no-store");
      server_.send(200,"application/json","{\"nonce\":\""+String(value)+"\"}");
    });
    server_.on("/v1/memo",HTTP_POST,[this](){receive();});
  }
  bool pair(bool enable) {
    if(!enable){if(prefs_.isKey("key")&&!prefs_.remove("key"))return false;key="";server_.stop();running_=false;return true;}
    char value[33];for(int i=0;i<4;i++)snprintf(value+i*8,9,"%08lx",(unsigned long)esp_random());
    if(prefs_.putString("key",value)!=32)return false;key=value;
    challenges_.reset();return true;
  }
  void tick(bool connected) {
    if(connected&&key.length()==32){if(!running_){server_.begin();running_=true;}server_.handleClient();}
    else if(running_){server_.stop();running_=false;}
  }
 private:
  WebServer server_{8080};Preferences prefs_;Save save_;bool running_=false;
  PassportChallenges challenges_;
  static String mac(const String& key,const String& data){
    uint8_t sum[32];mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
      (const unsigned char*)key.c_str(),key.length(),(const unsigned char*)data.c_str(),data.length(),sum);
    char result[65];for(int i=0;i<32;i++)snprintf(result+i*2,3,"%02x",sum[i]);return result;
  }
  static bool equal(const String& a,const String& b){if(a.length()!=b.length())return false;unsigned diff=0;for(size_t i=0;i<a.length();i++)diff|=a[i]^b[i];return diff==0;}
  void receive(){
    const String& body=server_.arg("plain");JsonDocument doc;
    if(key.length()!=32||body.length()>2048||deserializeJson(doc,body)||
       !doc["id"].is<uint32_t>()||!doc["done"].is<bool>()||!doc["text"].is<String>()||!doc["nonce"].is<String>()||!doc["mac"].is<String>()){
      server_.send(400,"text/plain","Invalid request");return;
    }
    uint32_t id=doc["id"];bool done=doc["done"];String text=doc["text"],nonce=doc["nonce"],signature=doc["mac"];std::string clean;
    if(!id||text.isEmpty()||normalizeNote(std::string(text.c_str(),text.length()),clean)){
      server_.send(400,"text/plain","Invalid note");return;
    }
    int match=challenges_.match(nonce.c_str(),millis());
    String canonical=passportCanonical(nonce.c_str(),id,done,text.c_str()).c_str();
    if(match<0||!equal(signature,mac(key,canonical))){server_.send(403,"text/plain","Invalid signature or expired challenge");return;}
    if(!save_||!save_(clean.c_str(),id,done)){server_.send(500,"text/plain","Storage failed");return;}
    challenges_.consume(match);
    server_.send(200,"application/json","{\"saved\":true,\"mac\":\""+mac(key,"saved\n"+nonce+"\n"+String(id))+"\"}");
  }
};
