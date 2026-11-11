#ifndef LURL_HPP
#define LURL_HPP

#include <vector>
#include <utility>
#include <stdexcept>
#include <cstdint>
#include <ostream>
#include <utility>
#include "lkcore/lstringutil.h"

LK_NAMESPACE_BEGIN

class LKCOREDEF LUrl {
public:
    // Default constructor
    LUrl() : m_parse(true),m_built(true),m_ipV(-1) {}

    // Copy initializer constructor
    LUrl(const LUrl &url) : m_ipV(-1) {assign(url);}

    // Move constructor
    LUrl(LUrl&& url) : m_ipV(-1) {assign(std::move(url));}

    // Construct LUrl with the given string
    LUrl(const std::string &url_str) : m_url(url_str),m_parse(false),m_built(false),m_ipV(-1) {}

    // Assign the given URL string
    LUrl &operator=(const std::string &url_str) {return str(url_str);}

    // Assign the given LUrl object
    LUrl &operator=(const LUrl &url) {assign(url); return *this;}

    // Move the given LUrl object
    LUrl &operator=(LUrl&& url) {assign(std::move(url)); return *this;}

    // Clear the LUrl object
    LUrl &clear();

    // Build LUrl if needed and return it as string
    std::string str() const {if(!m_built) buildUrl(); return m_url;}

    // Set the LUrl to the given string. All fields are overwritten
    LUrl& str(const std::string &url_str) {m_url=url_str; m_built=m_parse=false; return *this;}

    // Get scheme
    const std::string& scheme() const {lazyParse(); return m_scheme;}

    // Set scheme
    LUrl &setScheme(const std::string& s);

    // Get user info
    const std::string& userInfo() const {lazyParse(); return m_user;}

    // Set user info
    LUrl &setUserInfo(const std::string& s);

    // Get host
    const std::string& host() const {lazyParse(); return m_host;}

    // Set host
    LUrl &setHost(const std::string& h, uint8_t ip_v=0);

    // Get host IP version: 0=name, 4=IPv4, 6=IPv6, -1=undefined
    std::int8_t ipVersion() const {lazyParse(); return m_ipV;}

    // Get port
    const std::string& port() const {lazyParse(); return m_port;}
    const int portInt() const;

    // Set Port given as string
    LUrl &setPort(const std::string& str);

    // Set port given as a 16bit unsigned integer
    LUrl &setPort(std::uint16_t num) {return setPort(std::to_string(num));}

    // Get path
    const std::string& path() const {lazyParse(); return m_path;}

    // Set path
    LUrl &setPath(const std::string& str);

    class LKCOREDEF KeyVal {
    public:
        // Default constructor
        KeyVal() {}

        // Construct with provided Key and Value strings
        KeyVal(const std::string &key, const std::string &val) : m_key(key),m_val(val) {}

        // Construct with provided Key string, val will be empty
        KeyVal(const std::string &key) : m_key(key) {}

        // Equality test operator
        bool operator==(const KeyVal &q) const {return m_key==q.m_key&&m_val==q.m_val;}

        // Swap this with q
        void swap(KeyVal& q) {std::swap(m_key,q.m_key); std::swap(m_val,q.m_val);}

        // Get key
        const std::string& key() const {return m_key;}

        // Set key
        void key(const std::string &k) {m_key=k;}

        // Get value
        const std::string& val() const {return m_val;}

        // Set value
        void val(const std::string &v) {m_val=v;}

        // Output key value pair
        friend std::ostream& operator<<(std::ostream &o, const KeyVal &kv)
            {o<<"<key("<<kv.m_key<<") val("<<kv.m_val<<")> "; return o;}

    private:
        std::string m_key;
        std::string m_val;
    };

    // Define Query as vector of Key Value pairs
    typedef std::vector<KeyVal> Query;

    // Get a reference to the query vector for read only access
    const Query& query() const {lazyParse(); return m_query;}

    // Get a reference to a specific Key Value pair in the query vector for read only access
    const KeyVal& query(size_t i) const {
        lazyParse();
        if (i>=m_query.size())
            throw std::out_of_range("Invalid Url query index ("+std::to_string(i)+")");
        return m_query[i];
    }

    // Get a reference to the query vector for a writable access
    Query& setQuery() {lazyParse(); m_built=false; return m_query;}

    // Get a reference to specific Key Value pair in the query vector for a writable access
    KeyVal& setQuery(size_t i) {
        lazyParse();
        if (i>=m_query.size())
            throw std::out_of_range("Invalid Url query index ("+std::to_string(i)+")");
        m_built=false;
        return m_query[i];
    }

    // Set the query vector to the Query vector q
    LUrl &setQuery(const Query &q)
        {lazyParse(); if (q != m_query) {m_query=q; m_built=false;} return *this;}

    // Append KeyVal kv to the query
    LUrl &addQuery(const KeyVal &kv)
        {lazyParse(); m_built=false; m_query.push_back(kv); return *this;}

    // Append key val pair to the query
    LUrl &addQuery(const std::string &key, const std::string &val)
        {lazyParse(); m_built=false; m_query.emplace_back(key,val); return *this;}

    // Append key with empty val to the query
    LUrl &addQuery(const std::string &key)
        {lazyParse(); m_built=false; m_query.emplace_back(key); return *this;}

    // Get the fragment
    const std::string& fragment() const {lazyParse(); return m_fragment;}

    // Set the fragment
    LUrl &setFragment(const std::string& f);

    // Output
    std::ostream& output(std::ostream &o) const;

    // Output strean operator
    friend std::ostream& operator<<(std::ostream &o, const LUrl &u) {return u.output(o);}

    bool decodeDataUrl(std::string &mimeType, std::vector<uint8_t> &decodeData);


    enum UrlFormattingOption : unsigned int {
        None = 0x0,
        RemoveScheme = 0x01,
        RemoveUserInfo = 0x02,
        RemoveHost = 0x04,
        RemovePort = 0x08,
        RemovePath = 0x10,
        RemoveQuery = 0x20,
        RemoveFragment = 0x40
    };
    std::string formatUrl(uint8_t opt = None) const;
private:
    void assign(const LUrl &url);
    void assign(LUrl&& url);
    void buildUrl() const;
    void lazyParse() const {if (!m_parse) parseUrl();}
    void parseUrl() const;

    mutable std::string m_scheme;
    mutable std::string m_user;
    mutable std::string m_host;
    mutable std::string m_port;
    mutable std::string m_path;
    mutable Query m_query;
    mutable std::string m_fragment;
    mutable std::string m_url;
    mutable bool m_parse;
    mutable bool m_built;
    mutable std::int8_t m_ipV;
};

LK_NAMESPACE_END


#endif // URL_HPP

