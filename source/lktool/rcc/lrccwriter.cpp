#include "lrccwriter.h"
#include <stack>
#include <algorithm>
#include "lkcore/io/liofileman.h"
#include "lkcore/io/lxml.h"


LK_NAMESPACE_BEGIN

LRccWriter::LRccWriter() {}

bool LRccWriter::output(const std::string& file)
{
    std::filesystem::path path(file);
    if(m_outName.empty()){
        m_outName = path.stem().string();
    }

    LIOFileMan fileOut(path, std::ios_base::out|std::ios_base::binary);
    return output(fileOut);
}

bool LRccWriter::output(LIOMan &outMan)
{
    if(m_outName.empty()){
        m_outName = "lres";
    }

    if (!writeHeader()) {
        lWarn() << "Write rcc header failed!";
        return false;
    }
    if (m_root != nullptr) {
        sortFileInfos();

        if (!writeDataBlobs()) {
            lWarn() << "Write rcc data blobs failed!";
            return false;
        }
        if (!writeDataNames()) {
            lWarn() << "Write rcc data names failed!";
            return false;
        }
        if (!writeDataStructure()) {
            lWarn() << "Write rcc data structure failed!";
            return false;
        }
    }
    if (!writeInitializer()) {
        lWarn() << "Write rcc initializer failed!";
        return false;
    }

    outMan.writeRawData((uint8_t*)m_out.data(), (int64_t)m_out.size());
    return true;
}

bool LRccWriter::readFiles()
{
    for (const std::string& fName : m_rccFiles) {
        std::filesystem::path path(fName);

        if(path.is_absolute() == false){
            path = std::filesystem::absolute(path);
        }

        if(std::filesystem::exists(path)){
            if (!interpretResourceFile(path)){
                return false;
            }
        }
    }
    return true;
}

bool LRccWriter::addFileInfo(const std::string &alias, std::shared_ptr<LRccFileInfo> info)
{
    if(info == nullptr){
        lWarn() << "Null lrcc file info!";
        return false;
    }

    if(!info->m_path.is_absolute()){
        info->m_path = std::filesystem::absolute(info->m_path);
    }

    if(std::filesystem::is_directory(info->m_path)){
        lWarn() << "LRcc cannot support directory resources!";
        return false;
    }

    if (std::filesystem::file_size(info->m_path) > 0xffffffff) {
        lWarn() <<"LRcc file: [" <<info->m_path.string() << "] too big!" ;
        return false;
    }
    if (m_root == nullptr) {
        m_root = std::make_shared<LRccFileInfo>();
        m_root->m_isDir= true;;
    }

    std::shared_ptr<LRccFileInfo> parent = m_root;
    //lWarn() << "alias:"<< alias;
    const std::vector<std::string> nodes = LStringUtil::splitToVec(alias, "/");

    if(!nodes.empty()){
        for (int i = 1; i < nodes.size(); ++i) {
            const std::string& node = nodes.at(i);
            if (node.empty()){
                continue;
            }
            std::shared_ptr<LRccFileInfo> child = parent->findChild(node);

            if (child == nullptr) {
                std::shared_ptr<LRccFileInfo> s;
                if(i < (nodes.size()-1)){
                    s = std::make_shared<LRccFileInfo>();
                    s->m_isDir = true;
                }else{
                    s = info;
                    s->m_flags = 0;
                }
                //lWarn() << "Add node:" << node;
                s->m_name = node;
                s->m_parent = parent;
                parent->m_children.push_back(s);
                parent = s;
            } else {
                parent = child;
                if(i == (nodes.size()-1)){
                    if(child->m_isDir){
                        lWarn() << "Add resource file failed, already use node as directory!";
                        return false;
                    }else{
                        if(child->m_path != info->m_path){
                            lWarn() << "Add resource file failed, already use node as other file!";
                            return false;
                        }else{
                            return true;
                        }
                    }
                }
            }
        }
    }
    return true;
}

bool LRccWriter::interpretResourceFile(const std::filesystem::path& path)
{
    const char slash = '/';
    std::filesystem::path currentPath = path.parent_path();
    std::ifstream xmlFile(path);
    std::string   xmlContent{std::istreambuf_iterator<char>(xmlFile),std::istreambuf_iterator<char>()};


    LXmlParser parser(xmlContent);
    LXmlDoc tree;
    auto result = parser.to_tree(tree, true);

    if (result) {
        LXmlDoc::const_iterator it = tree.cbegin();
        it = it.dive();
        it = it.next("RCC");
        if(it != tree.cend()){
            it = it.dive().next("lresource");
            if(it != tree.cend()){
                std::string prefix(it.attribute("prefix"));
                if (!prefix.starts_with(slash)){
                    prefix = slash + prefix;
                }
                if (!prefix.ends_with(slash)){
                    prefix += slash;
                }

                it = it.dive().next("file");
                while(it != tree.cend()){
                    std::string file(it.dive().text());
                    if(!file.empty()){
                        std::shared_ptr<LRccFileInfo> info = std::make_shared<LRccFileInfo>();
                        info->m_path =  currentPath;
                        info->m_path /= file;
                        addFileInfo(prefix + file, info);
                    }
                    it = it.next();
                }
                return true;
            }
        }
    }
    else {
        lWarn() << "Error:" << result.error();
    }
    return false;
}

bool LRccWriter::writeHeader()
{
    switch (m_format) {
    case CppCode:{
        writeString("/****************************************************************************\n");
        writeString("** Resource object code\n");
        writeString("** Created by: The Resource Compiler for Looki");
        writeString("** WARNING! All changes made in this file will be lost!\n");
        writeString( "*****************************************************************************/\n\n");
        writeString("#ifndef " + LStringUtil::toUpper(m_outName) + "_H\n");
        writeString("#define " + LStringUtil::toUpper(m_outName) + "_H\n");
        writeString("#include \"lkcore/io/lresource.h\"\n");
        writeString("LK_NAMESPACE_BEGIN\n");
        writeString("namespace rcc_" + m_outName + "{\n");
        break;
    }
    case Binary:{
        writeString("lres");
        writeNumber(0);
        writeNumber(0);
        writeNumber(0);
        writeNumber(0);
        break;
    }
    default:
        break;
    }
    return true;
}

bool LRccWriter::writeDataBlobs()
{
    if (m_root == nullptr){
        return false;
    }

    switch (m_format) {
    case CppCode:{
        writeString("static const uint8_t lk_resource_data[] = {\n");
        break;
    }
    case Binary:{
        m_dataOffset = (int)m_out.size();
        break;
    }
    default:
        break;
    }

    std::stack<std::shared_ptr<LRccFileInfo> > pending;
    pending.push(m_root);
    int32_t offset = 0;
    while (!pending.empty()) {
        std::shared_ptr<LRccFileInfo> file = pending.top();
        pending.pop();
        for (const std::shared_ptr<LRccFileInfo>& child : file->m_children) {
            if (child->m_isDir){
                pending.push(child);
            }else {
                offset = writeDataBlob(child, offset);
                if (offset == 0) {
                    return false;
                }
            }
        }
    }
    switch (m_format) {
    case CppCode:{
        writeString("\n};\n\n");
        break;
    }
    default:
        break;
    }
    return true;

}

bool LRccWriter::writeDataNames()
{
    if (m_root == nullptr){
        return false;
    }


    switch (m_format) {
    case CppCode:{
        writeString("static const uint8_t lk_resource_name[] = {\n");
        break;
    }
    case Binary:{
        m_namesOffset = (int)m_out.size();
        break;
    }
    default:
        break;
    }

    std::stack<std::shared_ptr<LRccFileInfo> > pending;

    pending.push(m_root);
    int32_t offset = 0;
    while (!pending.empty()) {
        std::shared_ptr<LRccFileInfo> file = pending.top();
        pending.pop();
        for (const std::shared_ptr<LRccFileInfo>& child : file->m_children) {
            if (child->m_isDir){
                pending.push(child);
            }
            offset = writeDataName(child, offset);
        }
    }
    switch (m_format) {
    case CppCode:{
        writeString("\n};\n\n");
        break;
    }
    default:
        break;
    }
    return true;
}

bool LRccWriter::writeDataStructure()
{
    if (m_root == nullptr){
        return false;
    }


    switch (m_format) {
    case CppCode:{
        writeString("static const uint8_t lk_resource_struct[] = {\n");
        break;
    }
    case Binary:{
        m_treeOffset = (int)m_out.size();
        break;
    }
    default:
        break;
    }

    std::stack<std::shared_ptr<LRccFileInfo> > pending;

    //calculate the child offsets (flat)
    pending.push(m_root);
    int offset = 1;
    while (!pending.empty()) {
        std::shared_ptr<LRccFileInfo> file = pending.top();
        pending.pop();
        file->m_childOffset = offset;
        for (const std::shared_ptr<LRccFileInfo>& child : file->m_children) {
            ++offset;
            if (child->m_isDir){
                pending.push(child);
            }
        }
    }

    //write out the structure (ie iterate again!)
    pending.push(m_root);
    writeDataInfo(m_root);
    while (!pending.empty()) {
        std::shared_ptr<LRccFileInfo> file = pending.top();
        pending.pop();
        //write out the actual data now
        for (const std::shared_ptr<LRccFileInfo>& child : file->m_children) {
            writeDataInfo(child);
            if (child->m_isDir){
                pending.push(child);
            }
        }
    }
    switch (m_format) {
    case CppCode:{
        writeString("\n};\n\n");
        break;
    }
    default:
        break;
    }

    return true;

}

bool LRccWriter::writeInitializer()
{
    if (m_format == CppCode) {
        writeString("struct initializer {\n  ");
        writeString("initializer(){LResource::registResourceData(lk_resource_struct, lk_resource_name, lk_resource_data);}\n  ");
        writeString("~initializer(){LResource::unregistResourceData(lk_resource_struct, lk_resource_name, lk_resource_data);}\n");
        writeString("} dummpy; \n");
        writeString("}\n");
        writeString("LK_NAMESPACE_END\n");
        writeString("#endif");
    } else if (m_format == Binary) {
        uint8_t *p = (uint8_t*)m_out.data();
        int i = 4;
        p[i++] = (m_treeOffset >>  0) & 0xff;
        p[i++] = (m_treeOffset >>  8) & 0xff;
        p[i++] = (m_treeOffset >> 16) & 0xff;
        p[i++] = (m_treeOffset >> 24) & 0xff;

        p[i++] = (m_dataOffset >>  0) & 0xff;
        p[i++] = (m_dataOffset >>  8) & 0xff;
        p[i++] = (m_dataOffset >> 16) & 0xff;
        p[i++] = (m_dataOffset >> 24) & 0xff;

        p[i++] = (m_namesOffset >>  0) & 0xff;
        p[i++] = (m_namesOffset >>  8) & 0xff;
        p[i++] = (m_namesOffset >> 16) & 0xff;
        p[i++] = (m_namesOffset >> 24) & 0xff;
    }
    return true;
}

int32_t LRccWriter::writeDataBlob(std::shared_ptr<LRccFileInfo> info, int32_t offset)
{
    const bool text = m_format == CppCode;
    const bool binary = m_format == Binary;

    //capture the offset
    info->m_dataOffset = offset;
    std::vector<uint8_t> data;

    if (std::filesystem::exists(info->m_path)) {
        LIOFileMan file(std::filesystem::absolute(info->m_path), std::ios_base::in | std::ios_base::binary);
        if (!file.open()) {
            return 0;
        }
        data = file.readAll();
    }

    if(!data.empty()){
        if (m_compress && data.size()>=131) { //太小的文件压缩反而变大
            std::vector<uint8_t> temp = LIOMan::compressData(data);
            if(temp.size() < data.size()){
                info->m_isCompress = 1;
                data = std::move(temp);
            }
        }else{
            info->m_isCompress = 0;
        }
    }

    // some info
    if (text) {
        writeString("  // ");
        writeString(info->m_name);
        writeString("\n  ");
    }

    // write the length
    writeNumber4((uint32_t)data.size());
    if (text){
        writeString("\n  ");
    }
    offset += 4;

    // write the payload
    if (text) {
        const uint8_t *p = data.data();
        for (int i = (int)data.size(), j = 0; --i >= 0; ) {
            writeHex(*p++);
            if (j == 15) {
                writeString("\n  ");
                j = 0;
            }else{
                ++j;
            }
        }
    } else if (binary) {
        writeByteArray(data);
    }
    offset += (uint32_t)data.size();

    // done
    if (text){
        writeString("\n  ");
    }
    return offset;

}

int32_t LRccWriter::writeDataName(std::shared_ptr<LRccFileInfo> info, int32_t offset)
{
    const bool text = m_format == CppCode;
    // capture the offset
    info->m_nameOffset = offset;

    // some info
    if (text) {
        writeString("  // ");
        writeString(info->m_name);
        writeString("\n  ");
    }

    // write the length
    writeNumber2((uint16_t)info->m_name.size());
    if (text){
        writeString("\n  ");
    }
    offset += 2;

    // write the hash
    writeNumber4(hashCase(info->m_name));
    if (text){
        writeString("\n  ");
    }
    offset += 4;

    for (int i = 0; i < info->m_name.size(); ++i) {
        writeNumber1((uint8_t)info->m_name.at(i));
        if (text && ((i+1) % 32) == 0 ){
            writeString("\n  ");
        }
    }
    offset += (uint32_t)info->m_name.size();
    // done
    if (text ){
        writeString("\n  ");
    }
    return offset;

}

void LRccWriter::writeDataInfo(std::shared_ptr<LRccFileInfo> info)
{
    const bool text = m_format == CppCode;
    //some info
    if (text) {
        writeString("  // ");
        writeString(info->m_name);
        writeString("\n  ");
    }

    //pointer data
    if (info->m_isDir) {
        // name offset
        writeNumber4(info->m_nameOffset);
        // child count
        writeNumber4((uint32_t)info->m_children.size());
        // first child offset
        writeNumber4(info->m_childOffset);
    } else {
        // name offset
        writeNumber4(info->m_nameOffset);
        // flags
        if(info->m_isCompress){
            writeNumber4(uint32_t(-2));
        }else{
            writeNumber4(uint32_t(-1));
        }
        //data offset
        writeNumber4(info->m_dataOffset);
    }
    if (text){
        writeChar('\n');
    }
}

void LRccWriter::writeDecimal(int value)
{
    char buf[std::numeric_limits<int>::digits10 + 2];
    int n = snprintf(buf, sizeof(buf), "%d", value);
    write((uint8_t*)buf, n);
}


static const char hexDigits[] = "0123456789abcdef";
inline void LRccWriter::write2HexDigits(uint8_t number)
{
    writeChar(hexDigits[number >> 4]);
    writeChar(hexDigits[number & 0xf]);

}

void LRccWriter::writeHex(uint8_t number)
{
    writeChar('0');
    writeChar('x');
    if (number < 16)
        writeChar(hexDigits[number]);
    else{
        write2HexDigits(number);
    }
    writeChar(',');
}

void LRccWriter::write(const uint8_t *str, int len)
{
    size_t n = m_out.size();
    m_out.resize(n + len);
    memcpy(m_out.data() + n, str, len);
}


void LRccWriter::sortFileInfos()
{
    if (m_root == nullptr){
        return;
    }
    struct
    {
        bool operator()(const std::shared_ptr<LRccFileInfo>& a, const std::shared_ptr<LRccFileInfo>& b) const
        {
            return hashCase(a->name()) < hashCase(b->name());
        }
    } rccCompare;

    std::stack<std::shared_ptr<LRccFileInfo> > pending;
    pending.push(m_root);
    while (!pending.empty()) {
        std::shared_ptr<LRccFileInfo> file = pending.top();
        pending.pop();
        file->m_children.sort(rccCompare);

        //write out the actual data now
        for (const std::shared_ptr<LRccFileInfo>& child : file->m_children) {
            if (child->m_isDir){
                pending.push(child);
            }
        }
    }
}



LK_NAMESPACE_END
