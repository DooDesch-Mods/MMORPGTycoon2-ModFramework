bool read_initial_save_world(void* receiver, std::string& folder, std::string& error) {
    folder.clear();
    if(!check_thread(error))return false;
    const auto* game=singleton(game_slot);
    const auto* mode=singleton(mode_slot);
    void* active=nullptr;void* world=nullptr;int state=-1;unsigned char quitting=1;
    if(!has_vtable(game,0x1743730) || !read_at(game,0x28,active) || active!=mode || receiver!=mode ||
        !has_vtable(mode,0x16e6720) || !read_at(mode,0x58,world) || !world ||
        !read_at(mode,0x2c4,state) || state!=3 || !read_at(mode,0x304,quitting) || quitting ||
        !read_game_string(game,0x470,folder) || !net::snapshot_world_path::valid_folder(folder)) {
        folder.clear();return fail(error,"initial save requires the exact current state3 world and folder");
    }
    error.clear();return true;
}
