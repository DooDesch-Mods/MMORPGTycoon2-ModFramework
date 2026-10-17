void* native_pane(void* window, const std::string& id) {
    void* result = nullptr; check(string_call(0x81d860, window, id.c_str(), &result), "Native app pane " + id);
    require(result, "Native app pane missing: " + id); return result;
}
