#include "writable.h"

using UniqueHKey = std::unique_ptr<std::remove_pointer<HKEY>::type, decltype(&RegCloseKey)>;

void test(const Napi::CallbackInfo& info){
    std::cout << "test" << std::endl;
}

std::unique_ptr<wchar_t[]> utf8ToWideChar(const std::string& utf8) {
  int wide_char_length = MultiByteToWideChar(CP_UTF8,
    0,
    utf8.c_str(),
    -1,
    nullptr,
    0);
  if (wide_char_length == 0) return {};


  auto result = std::make_unique<wchar_t[]>(wide_char_length);
  if (MultiByteToWideChar(CP_UTF8,
    0,
    utf8.c_str(),
    -1,
    result.get(),
    wide_char_length) == 0)return {};

  return result;
}

// HKEY GetRootFromName(std::string root_name){
//     HKEY root = nullptr;
//     if (_stricmp(root_name.c_str(), "HKEY_CLASSES_ROOT") == 0 || _stricmp(root_name.c_str(), "HKCR") == 0) root = HKEY_CLASSES_ROOT;
//     else if (_stricmp(root_name.c_str(), "HKEY_CURRENT_USER") == 0 || _stricmp(root_name.c_str(), "HKCU") == 0) root = HKEY_CURRENT_USER;
//     else if (_stricmp(root_name.c_str(), "HKEY_LOCAL_MACHINE") == 0 || _stricmp(root_name.c_str(), "HKLM") == 0) root = HKEY_LOCAL_MACHINE;
//     else if (_stricmp(root_name.c_str(), "HKEY_USERS") == 0 || _stricmp(root_name.c_str(), "HKU") == 0) root = HKEY_USERS;
//     else if (_stricmp(root_name.c_str(), "HKEY_CURRENT_CONFIG") == 0 || _stricmp(root_name.c_str(), "HKCC") == 0) root = HKEY_CURRENT_CONFIG;
//     return root;
// }

HKEY GetRootFromName(std::string name) {
    // 1. 统一转为大写，这样只需要匹配一次，不需要多次调用 _stricmp
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);

    // 2. 静态映射表：只在第一次调用时初始化，性能极佳
    static const std::unordered_map<std::string, HKEY> rootMap = {
        {"HKCR", HKEY_CLASSES_ROOT},   {"HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT},
        {"HKCU", HKEY_CURRENT_USER},   {"HKEY_CURRENT_USER", HKEY_CURRENT_USER},
        {"HKLM", HKEY_LOCAL_MACHINE},  {"HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE},
        {"HKU",  HKEY_USERS},          {"HKEY_USERS", HKEY_USERS},
        {"HKCC", HKEY_CURRENT_CONFIG}, {"HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG}
    };

    // 3. 查表返回
    auto it = rootMap.find(name);
    return (it != rootMap.end()) ? it->second : nullptr;
}

std::pair<std::unique_ptr<char[]>, std::unique_ptr<char[]>> ParseSubkeyAndValue(const std::string& subPath) {
    const auto lastSlash = subPath.rfind('\\');
    if (lastSlash == std::string::npos || lastSlash + 1 >= subPath.size()) return { nullptr, nullptr };

    const std::string subkeyArg = subPath.substr(0, lastSlash);
    const std::string valueArg  = subPath.substr(lastSlash + 1);

    auto make_buffer = [](const std::string& s) -> std::unique_ptr<char[]> {
        auto buf = std::make_unique<char[]>(s.size() + 1);
        std::memcpy(buf.get(), s.data(), s.size());
        buf[s.size()] = '\0';
        return buf;
    };

    return {
        make_buffer(subkeyArg),
        make_buffer(valueArg)
    };
}

// 在头文件或函数声明处设置默认参数 KEY_READ
std::pair<UniqueHKey, LSTATUS> OpenRegKey(
    HKEY root,
    const std::string& path,
    REGSAM open_mode = KEY_READ  // 默认权限为只读
) {
    auto path_w = utf8ToWideChar(path);
    if (!path_w) {
        return { UniqueHKey(nullptr, RegCloseKey), ERROR_INVALID_PARAMETER };
    }

    HKEY raw = nullptr;
    LSTATUS ret = RegOpenKeyExW(root, path_w.get(), 0, open_mode, &raw);

    return { UniqueHKey(raw, RegCloseKey), ret };
}

// 一个简单的辅助宏，让代码读起来更像 JS 的风格
#define THROW_JS_ERROR(env, msg) \
    Napi::TypeError::New(env, msg).ThrowAsJavaScriptException(); \
    return env.Undefined();

Napi::Value ReadFromKey(const Napi::CallbackInfo& info) {
    const Napi::Env& env = info.Env();

    // 1. 基础参数验证：卫语句风格，清爽！
    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString()) {
        THROW_JS_ERROR(env, "Expected 2 string arguments: <HIVE>, <SUBKEY>\\\\<VALUE>");
    }

    // 2. 解析根键与输入路径
    HKEY root = GetRootFromName(info[0].As<Napi::String>());
    if (!root) {
        THROW_JS_ERROR(env, "Unknown hive specified.");
    }

    std::string full_path = info[1].As<Napi::String>();

    auto [subKeyPath, valueName] = ParseSubkeyAndValue(full_path);
    bool parsedValue = subKeyPath && valueName;
    if (!parsedValue) {
        auto make_buffer = [](const std::string& s) -> std::unique_ptr<char[]> {
            auto buf = std::make_unique<char[]>(s.size() + 1);
            std::memcpy(buf.get(), s.data(), s.size());
            buf[s.size()] = '\0';
            return buf;
        };
        subKeyPath = make_buffer(full_path);
        valueName = std::make_unique<char[]>(1);
        valueName[0] = '\0';
    }

    auto [hKey, openStatus] = OpenRegKey(root, subKeyPath.get());
    if (openStatus != ERROR_SUCCESS) {
        std::string err_msg = "Cannot open registry key. Error code: " + std::to_string(openStatus);
        THROW_JS_ERROR(env, err_msg);
    }

    auto valueNameW = utf8ToWideChar(valueName.get());
    if (!valueNameW) {
        THROW_JS_ERROR(env, "Failed to parse value name to wide string.");
    }

    DWORD dataType = 0, dataSize = 0;
    LSTATUS queryStatus = RegQueryValueExW(hKey.get(), valueNameW.get(), nullptr, &dataType, nullptr, &dataSize);

    if (queryStatus == ERROR_FILE_NOT_FOUND && parsedValue) {
        auto [defaultKey, defaultOpenStatus] = OpenRegKey(root, full_path);
        if (defaultOpenStatus == ERROR_SUCCESS) {
            hKey = std::move(defaultKey);
            valueNameW = utf8ToWideChar("");
            queryStatus = RegQueryValueExW(hKey.get(), valueNameW.get(), nullptr, &dataType, nullptr, &dataSize);
        }
    }

    if (queryStatus == ERROR_FILE_NOT_FOUND) {
        THROW_JS_ERROR(env, "The specified registry value does not exist.");
    } else if (queryStatus != ERROR_SUCCESS) {
        THROW_JS_ERROR(env, "RegQueryValueEx failed. Code: " + std::to_string(queryStatus));
    }

    auto buffer = std::make_unique<BYTE[]>(dataSize > 0 ? dataSize : 1);
    RegQueryValueExW(hKey.get(), valueNameW.get(), nullptr, &dataType, buffer.get(), &dataSize);

    // 7. 组装返回对象
    auto result = Napi::Object::New(env);

    // 设置名称
    result.Set("name", Napi::String::New(env, reinterpret_cast<char16_t*>(valueNameW.get()), wcslen(valueNameW.get())));

    // 设置类型字符串（简洁的映射方式）
    const char* typeStr = "UNKNOWN";
    switch (dataType) {
        case REG_SZ:        typeStr = "REG_SZ"; break;
        case REG_EXPAND_SZ: typeStr = "REG_EXPAND_SZ"; break;
        case REG_DWORD:     typeStr = "REG_DWORD"; break;
        case REG_BINARY:    typeStr = "REG_BINARY"; break;
    }
    result.Set("type", typeStr);

    // 8. 处理数据内容：按类型分发逻辑
    if (dataType == REG_SZ || dataType == REG_EXPAND_SZ) {
        auto* wData = reinterpret_cast<wchar_t*>(buffer.get());
        size_t wCharLen = dataSize / sizeof(wchar_t);
        // 去除 Windows 注册表常见的末尾空终止符
        if (wCharLen > 0 && wData[wCharLen - 1] == L'\0') wCharLen--;

        result.Set("data", Napi::String::New(env, reinterpret_cast<char16_t*>(wData), wCharLen));
    }
    else if (dataType == REG_DWORD && dataSize >= sizeof(DWORD)) {
        DWORD dwValue = *reinterpret_cast<DWORD*>(buffer.get());
        result.Set("data", Napi::Number::New(env, static_cast<uint32_t>(dwValue)));
    }
    else {
        // 对于不支持的类型，我们返回原始 Buffer 或报错
        THROW_JS_ERROR(env, "Unsupported registry value type for automatic conversion.");
    }

    return result;
}


Napi::Value DeleteFromPath(const Napi::CallbackInfo& info) {
    const Napi::Env& env = info.Env();

    // 1. 参数校验
    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString()) {
        THROW_JS_ERROR(env, "Expected 2 string arguments: <HIVE>, <SUBKEY>\\\\<VALUE>");
    }

    // 2. 获取根键
    HKEY root = GetRootFromName(info[0].As<Napi::String>());
    if (!root) {
        THROW_JS_ERROR(env, "Unknown hive specified.");
    }

    std::string input = info[1].As<Napi::String>();
    auto inputW = utf8ToWideChar(input);

    // 3. 尝试作为“子键(Subkey)”直接删除
    // RegDeleteTreeW 会递归删除该路径下的所有子项和值，非常暴力且好用
    LSTATUS status = RegDeleteTreeW(root, inputW.get());
    if (status == ERROR_SUCCESS) return Napi::Boolean::New(env, true);

    // 4. 如果直接删除失败，说明路径可能指向的是一个具体的“键值(Value)”
    // 我们需要解析出父路径和键值名
    auto [subKeyPath, valueName] = ParseSubkeyAndValue(input);

    // 打开父节点
    auto [hKey, openStatus] = OpenRegKey(root, subKeyPath.get(),KEY_SET_VALUE | KEY_QUERY_VALUE);
    if (openStatus != ERROR_SUCCESS) {
        // 父节点都打不开，说明路径彻底不存在，返回 false 或 throw 取决于你的偏好
        THROW_JS_ERROR(env, "Cannot open registry key. Error code: " + std::to_string(openStatus));
        return Napi::Boolean::New(env, false);
    }

    // 5. 执行键值删除
    auto valueNameW = utf8ToWideChar(valueName.get());
    status = RegDeleteValueW(hKey.get(), valueNameW.get());

    // 6. 返回结果：只有 ERROR_SUCCESS 才算删除成功
    return Napi::Boolean::New(env, status == ERROR_SUCCESS);
}
