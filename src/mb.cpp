/*
MIT License

Copyright (c) 2020 Phil Bowles with huge thanks to Adam Sharp http://threeorbs.co.uk
for testing, debugging, moral support and permanent good humour.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include"mb.h"

PANG_MEM_POOL       mb::pool;

mb::mb(size_t l,uint8_t* d,uint16_t i,ADFP f,bool track): managed(track){ // always unmanaged - sould only be called by onData
	len=l;
	data=d;
	frag=f;
	id=i;
    manage();
}

mb::mb(ADFP p, bool track): managed(track) {
    data=p;
    uint32_t multiplier = 1;
    uint32_t value = 0;
    uint8_t encodedByte;//,rl=0;
    ADFP pp=&data[1];
    do{
        encodedByte = *pp++;
        offset++;
        value += (encodedByte & 0x7f) * multiplier;
        multiplier *= 128;
    } while ((encodedByte & 0x80) != 0);
    len=1+offset+value; // MQTT msg size
    manage();
//  type 0x30 only
    if(isPub()){
        uint8_t bits=data[0] & 0x0f;
        dup=(bits & 0x8) >> 3;
        qos=(bits & 0x6) >> 1;
        retain=bits & 0x1;

        uint8_t* ps=start();
        id=0;
        size_t tlen=PANGO::_peek16(ps);ps+=2;
        char c_topic[tlen+1];
        memcpy(&c_topic[0],ps,tlen);c_topic[tlen]='\0';
        topic.assign(&c_topic[0],tlen);
        ps+=tlen;
        if(qos) {
            id=PANGO::_peek16(ps);
            ps+=2;
        }
        plen=data+len-ps;
        payload=ps;
    }
}

void mb::ack(){
    if(frag){
//        PANGO_PRINT("**** FRAGGY MB %08X frag=%08X\n",data,frag);
        if((int) frag < 100) return; // some arbitrarily ridiculous max numberof _fragments
        data=frag; // reset data pointer to FIRST fragment, so whole block is freed
        _deriveQos(); // recover original QOS from base fragment
    } 
//    PANGO_PRINT("**** PROTOCOL ACK MB %08X TYPE %02X L=%d I=%d Q=%d F=%08X R=%d\n",data,data[0],len,id,qos,frag,retries);
    if(!(isPub() && qos)) clear();
//    else PANGO_PRINT("HELD MB %08X TYPE %02X L=%d I=%d Q=%d F=%08X R=%d\n",data,data[0],len,id,qos,frag,retries);
}

void mb::clear(){
    if(managed){
        if(pool.count(data)) {
 //           PANGO_PRINT("*********************************** FH=%u KILL POOL %08X %d remaining\n\n",PANGO::_HAL_getFreeHeap()),data,pool.size());
            if(data){
                free(data);
                pool.erase(data);
            } //else PANGO_PRINT("WARNING!!! ZERO MEM BLOCK %08X\n",z);
        }
//        else PANGO_PRINT("WARNING!!! DOUBLE DIP DELETE! %08X len=%d type=%02X id=%d\n",data,len,data[0],id);
    } //else PANGO_PRINT("UNMANAGED\n");
}

void mb::manage(){
//    PANGO_PRINT("MANAGE %08X\n",data);
    if(managed){
        if(!pool.count(data)) {
//            PANGO_PRINT("\n*********************************** FH=%u INTO POOL %08X TYPE %s len=%d IN Q %u\n",PANGO::_HAL_getFreeHeap()),data,PANGO::getPktName(data[0]),len,pool.size());
            pool.insert(data);
//            dump();
        } //else PANGO_PRINT("\n* NOT ***************************** INTO POOL %08X %d IN Q\n",p,pool.size());
    } //else PANGO_PRINT("* NOT MANAGED\n");
    _deriveQos();
}

#ifdef PANGO_DEBUG
void mb::dump(){
    if(data){
        PANGO_PRINT4("MB %08X TYPE %02X L=%d M=%d O=%d I=%d Q=%d F=%08X\n",data,data[0],len,managed,offset,id,qos,frag);
        PANGO::dumphex(data,len);
    } else PANGO_PRINT4("MB %08X ZL or bare: can't dump\n",data);
}
#else
void mb::dump(){}
#endif
