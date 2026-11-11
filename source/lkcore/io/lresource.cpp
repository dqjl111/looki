#include "lresource.h"
#include "lkcore/ldebug.h"
#include <shared_mutex>
#include "lkcore/io/lendian.h"
#include "lkcore/io/lioman.h"
LK_NAMESPACE_BEGIN


struct LResNode{
    int nameOffset; //文件名偏移
    int childCount; // >=0为目录, <0 为文件(-1无压缩， -2有压缩)
    int offset; // 目录时，代表子文件列表的, 文件时代表数据的偏移
};


class LResRoot : public LIResRoot{
protected:
    const uint8_t *tree = nullptr;
    const uint8_t *names = nullptr;
    const uint8_t* payloads = nullptr;

    inline int findOffset(const int& node) const { return node * 12; }
    inline void setSource(const uint8_t *t, const uint8_t *n, const uint8_t *d) {
        tree = t;
        names = n;
        payloads = d;
    }
public:
    LResRoot(){}
    LResRoot(const uint8_t *t, const uint8_t *n, const uint8_t *d): tree(t), names(n), payloads(d){
    }

    virtual ~LResRoot(){}
    virtual ResType type() const override { return ResBuiltin; }

    static inline const int fetchNameOffset(const uint8_t* pNode){
        return lFromLittleEndian<int>(pNode);
    }

    static inline const int fetchChildCount(const uint8_t* pNode){
        return lFromLittleEndian<int>(pNode+4);
    }


    static inline const int fetchDataOffset(const uint8_t* pNode){
        return lFromLittleEndian<int>(pNode+8);
    }

    inline uint32_t hash(int node) const
    {
        if (node == 0){ // root
            return 0;
        }
        const int nameOffset = fetchNameOffset(tree + findOffset(node));
        return lFromLittleEndian<uint32_t>(names + nameOffset + 2);
    }


    virtual std::string name(int node) const override
    {
        if (node <= 0){ // root
            return "";
        }

        const int nameOffset = fetchNameOffset(tree + findOffset(node));
        const uint8_t* pName = names + nameOffset;
        uint16_t nameLength = lFromLittleEndian<uint16_t>(pName);
        return std::string((char*)(pName+6), nameLength);
    }


    virtual const uint8_t *data(int node, int64_t *size) const override
    {
        if (node <=0) {
            *size = 0;
            return nullptr;
        }
        const uint8_t * pNode = tree + findOffset(node);
        if (fetchChildCount(pNode)<0) {
            const uint8_t* pData = payloads + fetchDataOffset(pNode);
            const uint32_t dataLength = lFromLittleEndian<uint32_t>(pData);
            const uint8_t *ret = pData + 4;
            *size = dataLength;
            return ret;
        }
        *size = 0;
        return nullptr;
    }

    virtual std::list<std::string> children(int node) const override
    {
        if (node <0){
            return std::list<std::string>();
        }

        const uint8_t * pNode = tree + findOffset(node);
        std::list<std::string> ret;
        int childCount = fetchChildCount(pNode);
        if (childCount>0) {
            int offset = fetchDataOffset(pNode);
            if(offset>0){
                int endNode =  fetchDataOffset(pNode) + childCount;
                for (int i = offset; i <endNode; ++i){
                    ret.push_back(name(i));
                }
            }
        }
        return ret;
    }

    virtual bool isCompressed(int node) const override{
        if (node <=0){
            return false;
        }

        const uint8_t * pNode = tree + findOffset(node);
        return fetchChildCount(pNode) == -2;
    }

    virtual bool isContainer(int node) const override{
        if (node <0){
            return false;
        }

        if(node == 0){
            return true;
        }

        const uint8_t * pNode = tree + findOffset(node);
        return fetchChildCount(pNode) >=0;
    }

    inline bool operator==(const LResRoot &other) const
    { return tree == other.tree && names == other.names && payloads == other.payloads; }
    inline bool operator!=(const LResRoot &other) const
    { return !operator==(other); }


    virtual int findNode(const std::string &_path) const override{
        std::string path = _path;
        {
            std::string root = mappingRoot();
            if (!root.empty()) {
                if (root == path) {
                    path = '/';
                } else {
                    if (!root.ends_with('/')){
                        root += '/';
                    }
                    if (path.size() >= root.size() && path.starts_with(root)){
                        path = path.substr(root.size() - 1);
                    }
                    if (path.empty()){
                        path = '/';
                    }
                }
            }
        }

        if (path == "/"){
            return 0;
        }

        // the root node is always first
        int32_t childCount = fetchChildCount(tree);
        int32_t child = fetchDataOffset(tree);

        //lWarn() << "childCount:" << childCount;

        // now iterate up the tree
        int node = -1;
        std::vector<std::string_view> segments;
        LStringUtil::split(path, '/', segments);
        int iSeg = 1;
        for(; (childCount >0) && iSeg < segments.size(); ++iSeg) {
            const std::string_view& segment = segments[iSeg];
            const uint32_t h = hashCase(segment);
            //lWarn() << "segment:" << segment << ", hash:" <<h << ", iSeg:"<< iSeg;

            // do the binary search for the hash
            int l = 0, r = childCount - 1;
            int subNode = (l + r + 1) / 2;
            while (r != l) {
                const uint32_t subNodeHash = hash(child + subNode);
                //lWarn() << "subNode" << child + subNode << ":"<< name(child+subNode) << ", hash:" << subNodeHash;
                if (h == subNodeHash){
                    break;
                }
                else if (h < subNodeHash){
                    r = subNode - 1;
                }
                else{
                    l = subNode;
                }
                subNode = (l + r + 1) / 2;
            }
            subNode += child;

            // now do the "harder" compares
            bool found = false;
            if (hash(subNode) == h) {
                while (subNode > child && hash(subNode - 1) == h){ // backup for collisions
                    --subNode;
                }
                for (; subNode < child + childCount && hash(subNode) == h;  ++subNode) { // here we go...
                    if (name(subNode) == segment) {
                        found = true;
                        const uint8_t* pNode = tree + findOffset(subNode);
                        if ((iSeg+1) >= segments.size()) {
                            return subNode;
                        }

                        childCount = fetchChildCount(pNode);
                        if (childCount <0){
                            return -1;
                        }
                        child = fetchDataOffset(pNode);
                        break;
                    }
                }
            }
            if (!found){
                break;
            }            
        }
        return node;
    }

};

class LDynBufferResRoot : public LResRoot
{
    std::string root;
    const uint8_t *buffer;
public:
    inline LDynBufferResRoot(const std::string &_root) : root(_root), buffer(nullptr) { }
    inline ~LDynBufferResRoot() { }
    inline const uint8_t *mappingBuffer() const { return buffer; }

    virtual ResType type() const override { return ResBuffer; }
    virtual  const std::string& mappingRoot() const override { return root; }

    // size == -1 means "unknown"
    bool registerSelf(const uint8_t *b, lsizetype size = -1)
    {
        // 5 int "pointers"
        if (size >=0 && size < 20){
            return false;
        }
        // setup the data now
        int offset = 0;

        // magic number
        if (b[offset + 0] != 'l' || b[offset + 1] != 'r' || b[offset + 2] != 'e'
            || b[offset + 3] != 's') {
            return false;
        }
        offset += 4;

        const int treeOffset = lFromLittleEndian<uint32_t>(b + offset);
        offset += 4;

        const int dataOffset = lFromLittleEndian<uint32_t>(b + offset);
        offset += 4;

        const int nameOffset = lFromLittleEndian<uint32_t>(b + offset);
        // offset += 8; //保留4字节扩展
        // Some sanity checking for sizes. This is _not_ a security measure.
        if(size>=0){
            if (treeOffset >= size || dataOffset >= size || nameOffset >= size){
                return false;
            }
        }
        buffer = b;
        setSource(b + treeOffset, b + nameOffset, b + dataOffset);
        return true;
    }
};




typedef std::list<std::shared_ptr<LResRoot>> LResourceList;
struct LResourceGlobalData
{
    std::shared_mutex resourceMutex;
    LResourceList resourceList;
};

static inline LResourceGlobalData& resouceGlobal(){
    static LResourceGlobalData data;
    return data;
}

static inline std::shared_mutex &resourceMutex()
{ return resouceGlobal().resourceMutex; }

static inline LResourceList& resourceList()
{ return resouceGlobal().resourceList; }


LResource::LResource(const std::string &fileName)
{
    m_fileName = fileName;
}


void LResource::registResourceData(const uint8_t *tree, const uint8_t *name, const uint8_t *data)
{
    std::unique_lock<std::shared_mutex> lock(resourceMutex());
    LResourceList& list = resourceList();
    std::shared_ptr<LResRoot> res = std::make_shared<LResRoot>(tree, name, data);
    for(const std::shared_ptr<LResRoot>& resI: list) {
        if (*resI == *res) {
            return;
        }
    }
    list.push_back(res);
}

void LResource::unregistResourceData(const uint8_t *tree, const uint8_t *name, const uint8_t *data)
{
    std::unique_lock<std::shared_mutex> lock(resourceMutex());
    std::shared_ptr<LResRoot> res = std::make_shared<LResRoot>(tree, name, data);
    LResourceList& list = resourceList();
    LResourceList::iterator iter = list.begin();
    for (; iter != list.end();) {
        if (**iter == *res) {
            iter = list.erase(iter);
        } else {
            ++iter;
        }
    }
}

void LResource::clear()
{
    m_size =-1;
    m_data = nullptr;
    m_isCompress = false;
}

void LResource::setFileName(const std::string &fileName)
{

    if(m_fileName != fileName){
        clear();
        m_fileName = fileName;
    }
}

std::string LResource::cleanPath(const std::string &fileName)
{
    std::string path =LStringUtil::replace("\\", "/", fileName);

    std::string_view viewPath(path);
    if(viewPath.ends_with('/')){
        viewPath = viewPath.substr(0, viewPath.length()-1);
    }

    if(viewPath.starts_with(":")){
        viewPath = viewPath.substr(1, viewPath.length()-1);
    }

    while(viewPath.starts_with("//")){
        viewPath = viewPath.substr(1, viewPath.length()-1);
    }

    if(viewPath.starts_with("/")){
        return std::string(viewPath);
    }else{
        return std::string("/") + std::string(viewPath);
    }
}

int64_t LResource::size() const
{
    ensureInitialized();
    return m_size;
}

const uint8_t *LResource::data() const
{
    ensureInitialized();
    return m_data;
}

bool LResource::isCompress() const
{
    ensureInitialized();
    return m_isCompress;
}

std::vector<uint8_t> LResource::uncompressedData() const
{
    ensureInitialized();
    if(m_data == nullptr){
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> temp(m_size);
    std::memcpy(temp.data(),m_data , m_size);

    if(m_isCompress){
        return LIOMan::uncompressData(temp);
    }else{
        return temp;
    }
}

void LResource::ensureInitialized() const
{
    if(m_size <0){
        const_cast<LResource*>(this)->load(m_fileName);
    }
}

bool LResource::load(const std::string &file)
{
    m_size = 0;
    m_data = nullptr;
    m_isCompress = false;
    std::shared_lock<std::shared_mutex> lock(resourceMutex());

    std::string cleaned = cleanPath(file);
    const LResourceList& list = resourceList();
    for (const std::shared_ptr<LResRoot>& res : list) {
        const int node = res->findNode(cleaned);
        if (node != -1) {
            bool isContainter = res->isContainer(node);
            if(isContainter == false){
                m_data = res->data(node, &m_size);
                m_isCompress = res->isCompressed(node);
                return true;
            }
        }
    }
    return false;
}


LK_NAMESPACE_END
