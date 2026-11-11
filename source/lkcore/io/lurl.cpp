#include "lurl.h"
#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <sstream>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <ranges>
#include <vector>
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN

namespace {

static const uint8_t tbl[256] = {
    0,0,0,0, 0,0,0,0,     // NUL SOH STX ETX  EOT ENQ ACK BEL
    0,0,0,0, 0,0,0,0,     // BS  HT  LF  VT   FF  CR  SO  SI
    0,0,0,0, 0,0,0,0,     // DLE DC1 DC2 DC3  DC4 NAK SYN ETB
    0,0,0,0, 0,0,0,0,     // CAN EM  SUB ESC  FS  GS  RS  US
    0x00,0x01,0x00,0x00, 0x01,0x20,0x01,0x01, // SP ! " #  $ % & '
    0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x08, //  ( ) * +  , - . /
    0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01, //  0 1 2 3  4 5 6 7
    0x01,0x01,0x04,0x01, 0x00,0x01,0x00,0x10, //  8 9 : ;  < = > ?
    0x02,0x01,0x01,0x01, 0x01,0x01,0x01,0x01, //  @ A B C  D E F G
    0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01, //  H I J K  L M N O
    0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01, //  P Q R S  T U V W
    0x01,0x01,0x01,0x00, 0x00,0x00,0x00,0x01, //  X Y Z [  \ ] ^ _
    0x00,0x01,0x01,0x01, 0x01,0x01,0x01,0x01, //  ` a b c  d e f g
    0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01, //  h i j k  l m n o
    0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01, //  p q r s  t u v w
    0x01,0x01,0x01,0x00, 0x00,0x00,0x01,0x00, //  x y z {  | } ~ DEL
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
};


inline bool isChar(char c, std::uint8_t mask) {
    return (tbl[static_cast<unsigned char>(c)]&mask) != 0;
}


inline bool isChars(const char* s, const char* e, std::uint8_t mask) {
    while(s!=e){
        if (!isChar(*s++,mask)){
            return false;
        }
    }
    return true;
}


inline bool isAlpha(char c) {
    return (c>='A'&&c<='Z')||(c>='a'&&c<='z');
}


inline bool isNum(char c) {
    return c>='0'&&c<='9';
}


inline bool isAlnum(char c) {
    return isAlpha(c)||isNum(c);
}


inline bool isHexdigit(char c) {
    return isNum(c)||(c>='A'&&c<='F')||(c>='a'&&c<='f');
}


inline bool isUint(const char *&s, const char *e, uint32_t max) {
    if (s==e || !isNum(*s)){
        return false;
    }
    const char *t=s;
    uint32_t val = *t++-'0';
    if (val){
        while(t!=e && isNum(*t)){
           val=val*10+(*t++-'0');
        }
    }
    if (val>max){
        return false;
    }
    s=t;
    return true;
}


inline char getHexDigit(char c) {
    if (c>='0'&&c<='9'){
        return c-'0';
    }
    if (c>='A'&&c<='F'){
        return c-'A'+10;
    }
    if (c>='a'&&c<='f'){
        return c-'a'+10;
    }
    return -1;
}


inline void toLower(std::string& s) {
    for(auto& c : s){
        if (c>='A' && c<='Z'){
            c |= 0x20;
        }
    }
}


inline const char* findFirstOf(const char *s, const char *e, const char *q) {
    for(; s!=e; ++s){
        for(const char *t=q; *t; ++t){
            if (*s==*t){
                return s;
            }
        }
    }
    return e;
}


inline const char* findChar(const char *s, const char *e, const char c) {
    while (s!=e && *s!=c){
        ++s;
    }
    return s;
}


inline bool isScheme(const char *s, const char *e)
{
    if (!s||!e||s==e||!isAlpha(*s)){
        return false;
    }
    char c;
    while(++s!=e){
        if (!isAlnum(c=*s)&&c!='+'&&c!='-'&&c!='.'){
            return false;
        }
    }
    return true;
}


inline bool isScheme(const std::string &s) {
    return isScheme(s.data(),s.data()+s.length());
}


std::string normalizeScheme(const char *b, const char *e) {
    std::string o(b,e-b);
    toLower(o);
    return o;
}


inline bool isIpv4(const char *s, const char *e) {
    size_t l=e-s;
    if ((l<7) || (l>254)){
        return false;
    }
    for (const char *p=s; p!=e; ++p){
        if (*p!='.'&&!isNum(*p)){
            return false;
        }
    }
    return true;
}


inline bool isIpv4(const std::string &s) {
    return isIpv4(s.data(),s.data()+s.length());
}


inline bool isValidIpv4(const char *s, const char *e) {
    return isUint(s,e,255) && s!=e && *s++=='.' &&
           isUint(s,e,255) && s!=e && *s++=='.' &&
           isUint(s,e,255) && s!=e && *s++=='.' &&
           isUint(s,e,255) && s==e;
}


inline bool isValidIpv4(const std::string &s) {
    return isValidIpv4(s.data(),s.data()+s.length());
}


inline bool isRegName(const char *s, const char *e) {
    return isChars(s, e, 0x01);
}


inline bool isRegName(const std::string &s) {
    return isRegName(s.data(),s.data()+s.length());
}


std::string normalizeRegName(const std::string& s) {
    std::string o(s);
    toLower(o); // see rfc 4343
    return o;
}


bool isIpv6(const char *s, const char *e) {
    size_t l=e-s;
    if ((l<2) || (l>254)){
        return false;
    }
    for (const char *p=s; p!=e; ++p){
        if (*p!=':'&&*p!='.'&&!isHexdigit(*p)){
            return false;
        }
    }
    return true;
}


inline bool isIpv6(const std::string &s) {
    return isIpv6(s.data(),s.data()+s.length());
}


bool isValidIpv6(const char *s, const char *e) {
    if ((e-s)>39||(e-s)<2){
        return false;
    }
    bool nullField=false;
    const char *b=s, *p=s;
    int nfields=0, ndigits=0;
    if (p[0]==':') {
        if (p[1]!=':'){
            return false;
        }
        nullField=true;
        b=(p+=2);
        if (p==e){
            return true;
        }
    }
    while(p!=e) {
        if (*p=='.') {
            return ((!nullField&&nfields==6)||(nullField&&nfields<7))&&isValidIpv4(b, e);
        } else if (*p==':') {
            if (ndigits==0) {
                if (nullField){
                    return false;
                }
                nullField=true;
            } else {
                ++nfields;
                ndigits=0;
            }
            b=++p;
        } else {
            if ((++ndigits>4) || !isHexdigit(*p++)){
                return false;
            }
        }
    }
    if (ndigits>0){
        ++nfields;
    }
    else {
        if (e[-1]==':') {
            if (e[-2]==':' && nfields<8){
                return true;
            }
            return false;
        }
    }
    return (!nullField&&nfields==8)||(nullField&&nfields<8);
}


inline bool isValidIpv6(const std::string &s) {
    return isValidIpv6(s.data(),s.data()+s.length());
}


std::string normalizeIPv6(const char *s, const char *e) {
    if (!isIpv6(s, e)){
        lWarn() << std::string("IPv6 ["+std::string(s,e-s)+"] is invalid"); return "";
    }
    if ((e-s)==2 && s[0]==':' && s[1]==':'){
        return std::string(s,e-s);
    }

    // Split IPv6 at colons
    const size_t token_size = 10;
    const char *p=s, *tokens[token_size];
    if (*p==':'){
        ++p;
    }
    if (e[-1]==':'){
        --e;
    }
    const char *b=p;
    size_t i=0;
    while (p!=e) {
        if (*p++==':') {
            if (i+1 >= token_size) {
                lWarn() << std::string("IPv6 ["+std::string(s,e-s)+"] is invalid"); return "";
            }
            tokens[i++]=b;
            b=p;
        }
    }
    if (i<8){
        tokens[i++]=b;
    }
    tokens[i]=p;
    size_t ntokens=i;

    // Get IPv4 address which is normalized by default
    const char *ipv4B=nullptr, *ipv4E=nullptr;
    if ((tokens[ntokens]-tokens[ntokens-1])>5) {
        ipv4B=tokens[ntokens-1];
        ipv4E=tokens[ntokens];
        --ntokens;
    }

    // Decode the fields
    const size_t fields_size = 8;
    std::uint16_t fields[fields_size];
    size_t nullPos=8, nullLen=0, nfields=0;
    for(size_t i=0; i<ntokens; ++i) {
        const char *p=tokens[i];
        if (p==tokens[i+1] || *p==':'){
            nullPos=i;
        }else {
            if (nfields >= fields_size) {
                lWarn() << std::string("IPv6 ["+std::string(s,e-s)+"] is invalid"); return "";
            }
            std::uint16_t field=getHexDigit(*p++);
            while (p!=tokens[i+1] && *p!=':')
                field=(field<<4)|getHexDigit(*p++);
            fields[nfields++]=field;
        }
    }
    i = nfields;
    nfields=(ipv4B)?6:8;
    if (i<nfields) {
        if (i<nullPos) {
            lWarn() << std::string("IPv6 ["+std::string(s,e-s)+"] is invalid"); return "";
        }
        size_t last=nfields;
        if (i!=nullPos){
            do{
                fields[--last]=fields[--i];
            }while (i!=nullPos);
        }
        do{
            fields[--last]=0;
        }while (last!=nullPos);
    }

    // locate first longer sequence of zero
    i=nullLen=0;
    nullPos=nfields;
    size_t first=0;
    for(;;) {
        while (i<nfields && fields[i]!=0){
            ++i;
        }
        if (i==nfields){
            break;
        }
        first=i;
        while (i<nfields && fields[i]==0){
            ++i;
        }
        if ((i-first)>nullLen) {
            nullPos=first;
            nullLen=i-first;
        }
        if (i==nfields){
            break;
        }
    }
    if (nullLen==1) {
        nullPos=nfields;
        nullLen=1;
    }

    // Encode normalized IPv6
    std::stringstream str;
    if (nullPos==0) {
        str << std::hex << ':';
        i=nullLen;
    } else {
        str << std::hex << fields[0];
        for (i=1; i<nullPos; ++i){
            str << ':' << fields[i];
        }
        if (i<nfields){
            str << ':';
        }
        i+=nullLen;
        if (i==8 && nullLen!=0){
            str << ':';
        }
    }
    for (; i<nfields; ++i){
        str << ':' << fields[i];
    }
    if (ipv4B){
        str << ':' << std::string(ipv4B, ipv4E-ipv4B);
    }

    return str.str();
}


inline std::string normalizeIPv6(const std::string &s) {
    return normalizeIPv6(s.data(),s.data()+s.length());
}


inline bool isPort(const char *s, const char *e) {
    return isUint(s,e,65535) && s==e;
}


inline bool isPort(const std::string &s) {
    return isPort(s.data(),s.data()+s.length());
}


std::string normalizePath(const std::string& s) {
    if (s.empty()){
        return s;
    }
    std::string elem;
    std::vector<std::string> elems;
    std::stringstream si(s);

    while(!std::getline(si, elem, '/').eof()){
        if (elem=="" || elem=="."){
            continue;
        }
        if (elem=="..") {
            if (!elems.empty()){
                elems.pop_back();
            }
            continue;
        }
        elems.push_back(elem);
    }
    if (elem=="."){
        elems.push_back("");
    }else if (elem=="..") {
        if (!elems.empty()){
            elems.pop_back();
        }
    }
    else{
        elems.push_back(elem);
    }

    std::stringstream so;
    if (s[0]=='/'){
        so << '/';
    }
    if (!elems.empty()) {
        auto it=elems.begin(), end=elems.end();
        so << *it;
        while(++it!=end){
            so << '/' << *it;
        }
    }
    return so.str();
}


std::string decode(const char *s, const char *e) {
    std::string o;
    o.reserve(e-s);
    while(s!=e) {
        char c=*s++, a=0, b=0;
        if (c=='%') {
            if (s==e || (a=getHexDigit(*s++))<0 || s==e || (b=getHexDigit(*s++))<0){
                lWarn() << std::string("Invalid percent encoding");
            }else{
                c=(a<<4)|b;
            }
        }
        o.push_back(c);
    }
    return o;
}


std::string decodePlus(const char *s, const char *e) {
    std::string o;
    o.reserve(e-s);
    while(s!=e) {
        char c=*s++, a=0, b=0;
        if (c=='+'){
            c=' ';
        }
        else if (c=='%') {
            if (s==e || (a=getHexDigit(*s++))<0 || s==e || (b=getHexDigit(*s++))<0){
                lWarn() << std::string("Invalid percent encoding");
            }
            c=(a<<4)|b;
        }
        o.push_back(c);
    }
    return o;
}


class LUrlEncode {
    public:
        LUrlEncode(const std::string& s, std::uint8_t mask) : m_s(s), m_mask(mask) {}
    private:
        const std::string& m_s;
        std::uint8_t m_mask;
    friend std::ostream& operator<< (std::ostream& o, const LUrlEncode& e) {
        for (const char c:e.m_s){
            if (isChar(c,e.m_mask)){
                o<<c;
            }else{
                o<<'%'<<"0123456789ABCDEF"[((uint8_t)c)>>4]<<"0123456789ABCDEF"[((uint8_t)c)&0xF];
            }
        }
        return o;
    }
};


class LUrlEncodeQueryKey {
    public:
        LUrlEncodeQueryKey(const std::string& s, std::uint8_t mask) : m_s(s), m_mask(mask) {}
    private:
        const std::string& m_s;
        std::uint8_t m_mask;
    friend std::ostream& operator<< (std::ostream& o, const LUrlEncodeQueryKey& e) {
        for (const char c:e.m_s){
            if (c==' '){
                o<<'+';
            }
            else if (c=='+'){
                o<<"%2B";
            }
            else if (c=='='){
                o<<"%3D";
            }
            else if (c=='&'){
                o<<"%26";
            }
            else if (c==';'){
                o<<"%3B";
            }
            else if (isChar(c,e.m_mask)){
                o<<c;
            }
            else{
                o<<'%'<<"0123456789ABCDEF"[((uint8_t)c)>>4]<<"0123456789ABCDEF"[((uint8_t)c)&0xF];
            }
        }
        return o;
    }
};


class LUrlEncodeQueryVal {
    public:
        LUrlEncodeQueryVal(const std::string& s, std::uint8_t mask) : m_s(s), m_mask(mask) {}
    private:
        const std::string& m_s;
        std::uint8_t m_mask;
    friend std::ostream& operator<< (std::ostream& o, const LUrlEncodeQueryVal& e) {
        for (const char c:e.m_s){
            if (c==' '){
                o<<'+';
            }
            else if (c=='+'){
                o<<"%2B";
            }
            else if (c=='&'){
                o<<"%26";
            }
            else if (c==';'){
                o<<"%3B";
            }
            else if (isChar(c,e.m_mask)){
                o<<c;
            }
            else{
                o<<'%'<<"0123456789ABCDEF"[((uint8_t)c)>>4]<<"0123456789ABCDEF"[((uint8_t)c)&0xF];
            }
        }
        return o;
    }
};



} // end of anonymous namnespace
// ---------------------------------------------------------------------


// Copy assignment
void LUrl::assign(const LUrl &url) {
    m_parse=url.m_parse;
    m_built=url.m_built;
    if (m_parse) {
        m_scheme=url.m_scheme;
        m_user=url.m_user;
        m_host=url.m_host;
        m_ipV=url.m_ipV;
        m_port=url.m_port;
        m_path=url.m_path;
        m_query=url.m_query;
        m_fragment=url.m_fragment;
    }
    if (!m_parse || m_built){
        m_url=url.m_url;
    }
}


// Move assignment
void LUrl::assign(LUrl&& url) {
    m_parse=url.m_parse;
    m_built=url.m_built;
    if (m_parse) {
        m_scheme=std::move(url.m_scheme);
        m_user=std::move(url.m_user);
        m_host=std::move(url.m_host);
        m_ipV=std::move(url.m_ipV);
        m_port=std::move(url.m_port);
        m_path=std::move(url.m_path);
        m_query=std::move(url.m_query);
        m_fragment=std::move(url.m_fragment);
    }
    if (!m_parse || m_built){
        m_url=std::move(url.m_url);
    }
}


LUrl &LUrl::setScheme(const std::string& s) {
    if (!isScheme(s)){
        lWarn() << std::string("Invalid scheme '"+s+"'");
        return *this;
    }
    lazyParse();
    std::string o(s);
    toLower(o);
    if (o!=m_scheme) {
        m_scheme=o;
        m_built=false;
        if ((m_scheme=="http" && m_port=="80") || (m_scheme=="https" && m_port=="443")){
            m_port="";
        }
    }
    return *this;
}


LUrl &LUrl::setUserInfo(const std::string& s) {
    if (s.length()>256){
        lWarn() << std::string("User info is longer than 256 characters '"+s+"'");
        return *this;
    }
    lazyParse();
    if (m_user!=s) {
        m_user=s;
        m_built=false;
    }
    return *this;
}


LUrl &LUrl::setHost(const std::string& h, std::uint8_t ip_v) {
    if (h.length()>253){
        lWarn() << std::string("Host is longer than 253 characters '"+h+"'");
        return *this;
    }
    lazyParse();
    std::string o;
    if (h.empty())
        ip_v=-1;
    else if (isIpv4(h)) {
        if (!isValidIpv4(h)){
            lWarn() << std::string("Invalid IPv4 address '"+h+"'");
            return *this;
        }
        ip_v=4;
        o=h;
    } else if(ip_v!=0&&ip_v!=4&&ip_v!=6) {
        if (!isIpv6(h)) {
            lWarn() << std::string("Invalid IPvFuture address '"+h+"'");
            return *this;
        }
        o=h;
    } else if (isIpv6(h)) {
        if (!isValidIpv6(h)){
            lWarn() << std::string("Invalid IPv6 address '"+h+"'");
            return *this;
        }
        ip_v=6;
        o=normalizeIPv6(h);
    } else if (isRegName(h)) {
        ip_v=0;
        o=normalizeRegName(h);
    } else{
        lWarn() << std::string("Invalid host '"+h+"'");
        return *this;
    }
    if (m_host!=o||m_ipV!=ip_v) {
        m_host=o;
        m_ipV=ip_v;
        m_built=false;
    }
    return *this;
}

const int LUrl::portInt() const
{
    lazyParse();
    std::istringstream iss(m_port);
    int num;
    if (iss >> num) {
        return num;
    } else {
        return -1;
    }
}


LUrl &LUrl::setPort(const std::string& p) {
    if (!isPort(p)){
        lWarn() << std::string("Invalid port '"+p+"'");
        return *this;
    }
    lazyParse();
    std::string o(p);
    if ((m_scheme=="http" && o=="80") || (m_scheme=="https" && o=="443")){
        o="";
    }
    if (m_port!=o) {
        m_port=o;
        m_built=false;
    }
    return *this;
}


LUrl &LUrl::setPath(const std::string& p) {
    if (p.length()>8000){
        lWarn() << std::string("Path is longer than 8000 characters '"+p+"'");
        return *this;
    }
    lazyParse();
    std::string o(normalizePath(p));
    if (m_path!=o) {
        m_path=o;
        m_built=false;
    }
    return *this;
}


LUrl &LUrl::setFragment(const std::string& f) {
    if (f.length()>256){
        lWarn() << std::string("Fragment is longer than 256 characters '"+f+"'");
        return *this;
    }
    lazyParse();
    if (m_fragment!=f) {
        m_fragment=f;
        m_built=false;
    }
    return *this;
}


LUrl &LUrl::clear() {
    m_url.clear();
    m_scheme.clear();
    m_user.clear();
    m_host.clear();
    m_port.clear();
    m_path.clear();
    m_query.clear();
    m_fragment.clear();
    m_ipV=-1;
    m_built=true;
    m_parse=true;
    return *this;
}


void LUrl::parseUrl() const {
    if (m_url.empty()) {
        const_cast<LUrl*>(this)->clear();
        m_parse=m_built=true;
        return;
    }
    if (m_url.length()>8000){
        lWarn() << std::string("URI is longer than 8000 characters");
        return;
    }

    const char *s=m_url.data(), *e=s+m_url.length();
    std::int8_t ipV=-1;
    const char *schemeB, *schemeE, *userB, *userE, *hostB, *hostE,
            *portB, *portE, *pathB, *pathE, *queryB, *queryE,
            *fragmentB, *fragmentE;
    schemeB=schemeE=userB=userE=hostB=hostE=portB=portE=pathB=
            pathE=queryB=queryE=fragmentB=fragmentE=nullptr;

    const char *b=s, *p=findFirstOf(b, e, ":/?#");
    if (p==e) {
        if (!isChars(b, p, 0x2F)){
            lWarn() << std::string("Path '"+std::string(b,p)+"' in '"+std::string(s,e-s)+"' is invalid"); return;
        }
        pathB=b;
        pathE=e;
    } else {
        // get schema if any
        if (*p==':') {
            if (!isScheme(b, p)){
                lWarn() << std::string("Scheme in '"+std::string(s,e-s)+"' is invalid"); return;
            }
            schemeB=b;
            schemeE=p;
            p=findFirstOf(b=p+1, e, "/?#");
        }
        // get authority if any
        if (p!=e && *p=='/' && (e-b)>1 && b[0]=='/' && b[1]=='/') {
            const char *ea=findFirstOf(b+=2, e, "/?#"); // locate end of authority
            p=findChar(b, ea, '@');
            // get user info if any
            if (p!=ea) {
                if (!isChars(b, p, 0x25)){
                    lWarn() << std::string("User info in '"+std::string(s,e-s)+"' is invalid"); return;
                }
                userB=b;
                userE=p;
                b=p+1;
            }
            // Get IP literal if any
            if (*b=='[') {
                // locate end of IP literal
                p=findChar(++b, ea, ']');
                if (*p!=']'){
                    lWarn() << std::string("Missing ] in '"+std::string(s,e-s)+"'");
                }
                // decode IPvFuture protocol version
                if (*b=='v') {
                    if (isHexdigit(*++b)) {
                        ipV=getHexDigit(*b);
                        if (isHexdigit(*++b)) {
                            ipV=(ipV<<8)|getHexDigit(*b);
                        }
                    }
                    if (ipV==-1||*b++!='.'||!isChars(b,p,0x05)){
                        lWarn() << std::string("Host address in '"+std::string(s,e-s)+"' is invalid"); return;
                    }
                } else if (isIpv6(b,p)) {
                    ipV=6;
                } else{
                    lWarn() << std::string("Host address in '"+std::string(s,e-s)+"' is invalid"); return;
                }
                hostB=b;
                hostE=p;
                b=p+1;
            } else {
                p=findChar(b, ea, ':');
                if (isIpv4(b, p)){
                    ipV=4;
                }
                else if (isRegName(b, p)){
                    ipV=0;
                }
                else{
                    lWarn() << std::string("Host address in '"+std::string(s,e-s)+"' is invalid"); return;
                }
                hostB=b;
                hostE=p;
                b=p;
            }
            //get port if any
            if (b!=ea&&*b==':') {
                if (!isPort(++b, ea)){
                    lWarn() << std::string("Port '"+std::string(b,ea-b)+"' in '"+std::string(s,e-s)+"' is invalid"); return;
                }
                portB=b;
                portE=ea;
            }
            b=ea;
        }
        p=findFirstOf(b,e,"?#");
        if (!isChars(b, p, 0x2F)){
            lWarn() << std::string("Path '"+std::string(b,p)+"' in '"+std::string(s,e-s)+"' is invalid"); return;
        }
        pathB=b;
        pathE=p;
        if (p!=e && *p=='?') {
            p=findChar(b=p+1,e,'#');
            queryB=b;
            queryE=p;
        }
        if (p!=e && *p=='#') {
            if (!isChars(p+1, e, 0x3F)){
                lWarn() << std::string("Fragment '"+std::string(p+1,e)+"' in '"+std::string(s,e-s)+"' is invalid"); return;
            }
            fragmentB=p+1;
            fragmentE=e;
        }
    }
    std::string _scheme, _user, _host, _port, _path, _query, _fragment;
    Query query_v;

    if (schemeB){
        _scheme=normalizeScheme(schemeB, schemeE);
    }
    if (userB){
        _user=decode(userB, userE);
    }
    if (hostB) {
        _host=decode(hostB, hostE);
        if (ipV==0){
            _host=normalizeRegName(_host);
        }
        else if (ipV==6){
            _host=normalizeIPv6(_host);
        }
    }
    if (portB){
        _port=std::string(portB,portE-portB);
    }
    if (pathB){
        _path=normalizePath(decode(pathB, pathE));
    }
    if (queryB) {
        _query=std::string(queryB, queryE);
        p=b=queryB;
        while (p!=queryE) {
            p=findFirstOf(b, queryE, "=;&");
            if (!isChars(b, p, 0x3F)){
                lWarn() << std::string("Query key '"+std::string(b,p)+"' in '"+std::string(s,e-s)+"' is invalid"); return;
            }
            std::string key(decodePlus(b,p)), val;
            if (p!=queryE) {
                if (*p=='=') {
                    p=findFirstOf(b=p+1, queryE, ";&");
                    if (!isChars(b, p, 0x3F)){
                        lWarn() << std::string("Query value '"+std::string(b,p)+"' in '"+std::string(s,e-s)+"' is invalid"); return;
                    }
                    val=decodePlus(b,p);
                }
                b=p+1;
            }
            query_v.emplace_back(key, val);
        }
    }
    if (fragmentB){
        _fragment=decode(fragmentB, fragmentE);
    }

    m_scheme=_scheme;
    m_user=_user;
    m_host=_host;
    m_ipV=ipV;
    m_port=_port;
    m_path=_path;
    m_query=query_v;
    m_fragment=_fragment;
    m_parse=true;
    m_built=false;
}


void LUrl::buildUrl() const {
    m_url = formatUrl();
    m_built=true;
}


// Output
std::ostream& LUrl::output(std::ostream &o) const {
    lazyParse();
    if(!m_built) buildUrl();
    o<<"Url:{url("<<m_url<<")";
    if (!m_scheme.empty()){
        o << " scheme("<<m_scheme<<")";
    }
    if (!m_user.empty()){
        o << " user_info("<<m_user<<")";
    }
    if (m_ipV!=-1){
        o << " host("<<m_host<<") IPv("<<(int)m_ipV<<")";
    }
    if (!m_port.empty()){
        o << " port("<<m_port<<")";
    }
    if (!m_path.empty()){
        o << " path("<<m_path<<")";
    }
    if (!m_query.empty()) {
        std::stringstream str;
        str<<" query(";
        for (const auto& q:m_query){
            str<<q;
        }
        std::string s(str.str());
        o<<s.substr(0,s.length()-1)<<")";
    }
    if (!m_fragment.empty()){
        o << "fragment("<<m_fragment<<") ";
    }
    o<<"}";
    return o;
}

bool LUrl::decodeDataUrl(std::string &mimeType, std::vector<uint8_t> &decodeData)
{
    /* https://www.rfc-editor.org/rfc/rfc2397.html

        data:[<mediatype>][;base64],<data>
        dataurl    := "data:" [ mediatype ] [ ";base64" ] "," data
        mediatype  := [ type "/" subtype ] *( ";" parameter )
        data       := *urlchar
        parameter  := attribute "=" value
    */

    if (m_scheme != "data" || !m_host.empty()){
        return false;
    }

    std::string payload = formatUrl(LUrl::RemoveScheme);
    // parse it:
    // percent decode after finding the `,`, to workaround parameter
    // values containing a percent-encoded comma
    const lsizetype pos = payload.find(',');
    if (pos != -1) {
        std::string contentType = decode(payload.data(), payload.data() + pos);
        auto data = std::string_view{contentType};
        data = LStringUtil::trim(data);

        std::string_view mime;
        std::string_view charsetParam;
        using namespace std::string_view_literals;

        constexpr auto charset = "charset"sv;
        bool first = true;
        auto caseInsensitiveEq = [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                   std::tolower(static_cast<unsigned char>(b));

        };

        for (auto t : data | std::views::split(";"sv)) {
            std::string_view part(&*t.begin(), std::ranges::distance(t));
            if (first) {
                if (part.contains('/')){
                    mime = part;
                }
                first = false;
            }
            // Minimal changes, e.g. if it's "charset=;" or "charset;" without
            // an encoding, leave it as-is
            if (std::ranges::starts_with(part, charset, caseInsensitiveEq)){
                charsetParam = part;
            }

            if (!mime.empty() && !charsetParam.empty()){
                break;
            }
        }

        if (mime.empty()) {
            mime = "text/plain";
            if (charsetParam.empty()){
                charsetParam = "charset=US-ASCII";
            }
        }
        if (!charsetParam.empty()){
            mimeType = std::string(mime) + ';' + std::string(charsetParam);
        }else{
            mimeType = mime;
        }

        // find out if the payload is encoded in Base64
        constexpr auto base64 = ";base64"sv; // per the RFC, at the end of `data`
        const bool isBas64 = std::ranges::ends_with(data, base64, caseInsensitiveEq);

        payload = payload.substr(pos + 1);
        data = {};
        payload = decode(payload.data(), payload.data() + payload.size());

        if (isBas64) {
            std::optional<std::vector<uint8_t> > r = LStringUtil::decodeFromBase64(payload);
            if (r == std::nullopt) {
                // just in case someone uses `payload` without checking the returned bool
                decodeData.clear();
                return false; // decoding failed
            }
            decodeData = r.value();
        }else{
            decodeData = std::vector<uint8_t>(payload.begin(), payload.end());
        }

        return true;
    } else {
        decodeData.clear();
    }

    return false;
}

std::string LUrl::formatUrl(uint8_t opt) const
{
    lazyParse();
    std::stringstream url;
    if (!m_scheme.empty() && (!(opt & RemoveScheme)))
        url<<m_scheme<<":";
    if (!m_host.empty() && (!(opt & RemoveHost))) {
        url<<"//";
        if (!m_user.empty() &&  (!(opt & RemoveUserInfo))){
            url<<LUrlEncode(m_user, 0x05)<<'@';
        }
        if (m_ipV==0||m_ipV==4){
            url<<m_host;
        }
        else if (m_ipV==6){
            url<<"["<<m_host<<"]";
        }
        else{
            url<<"[v"<<std::hex<<(int)m_ipV<<std::dec<<'.'<<m_host<<"]";
        }
        if (!m_port.empty() && (!(opt & RemovePort))){
            if (!((m_scheme=="http"&&m_port=="80")||(m_scheme=="https"&&m_port=="443"))){
                url<<":"<<m_port;
            }
        }
    } else {
        if (!m_user.empty()  && (!(opt & RemoveUserInfo))){
            lWarn() << std::string("User info defined, but host is empty");
            return url.str();
        }
        if (!m_port.empty()  && (!(opt & RemovePort))){
            lWarn() << std::string("Port defined, but host is empty");
            return url.str();
        }
        if (!m_path.empty()  && (!(opt & RemovePath))) {
            const char *b=m_path.data(), *e=b+m_path.length(), *p=findFirstOf(b,e,":/");
            if (p!=e && *p==':'){
                lWarn() << std::string("The first segment of the relative path can't contain ':'");
                return url.str();
            }
        }
    }
    if (!m_path.empty()   && (!(opt & RemovePath))) {
        if (m_path[0]!='/'&&!m_host.empty()){
            lWarn() << std::string("Path must start with '/' when host is not empty");
            return url.str();
        }
        url<<LUrlEncode(m_path, 0x0F);
    }
    if (!m_query.empty() && (!(opt & RemoveQuery))) {
        url<<"?";
        auto it = m_query.begin(), end = m_query.end();
        if (it->key().empty()){
            lWarn() << std::string("First query entry has no key");
            return url.str();
        }
        url<<LUrlEncodeQueryKey(it->key(), 0x1F);
        if (!it->val().empty()){
            url<<"="<<LUrlEncodeQueryVal(it->val(), 0x1F);
        }
        while(++it!=end) {
            if (it->key().empty()){
                lWarn() << std::string("A query entry has no key");
                return url.str();
            }
            url<<"&"<<LUrlEncodeQueryKey(it->key(), 0x1F);
            if (!it->val().empty()){
                url<<"="<<LUrlEncodeQueryVal(it->val(), 0x1F);
            }
        }
    }
    if (!m_fragment.empty()  && (!(opt & RemoveFragment))){
        url<<"#"<<LUrlEncode(m_fragment, 0x1F);
    }
    return url.str();
}

LK_NAMESPACE_END
