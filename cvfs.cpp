//
// Project: clibparser
// Created by bajdcc
//

#include <ctime>
#include <iterator>
#include <algorithm>
#include "cvfs.h"
#include "cexception.h"

namespace clib {

    void vfs_node_dec::advance() {
        if (available())
            idx++;
    }

    vfs_node_solid::vfs_node_solid(const vfs_node::ref &ref) : node(ref) {
        node.lock()->refs++;
    }

    vfs_node_solid::~vfs_node_solid() {
        node.lock()->refs--;
    }

    bool vfs_node_solid::available() const {
        auto n = node.lock();
        if (!n)
            return false;
        return idx < n->data.size();
    }

    int vfs_node_solid::index() const {
        auto n = node.lock();
        if (!n)
            return -2;
        if (idx < n->data.size())
            return n->data[idx];
        return -1;
    }

    vfs_node_cached::vfs_node_cached(const string_t &str) : cache(str) {}

    bool vfs_node_cached::available() const {
        return idx < cache.length();
    }

    int vfs_node_cached::index() const {
        return idx < cache.length() ? cache[idx] : -1;
    }

    cvfs::cvfs() {
        reset();
    }

    void cvfs::reset() {
        account.clear();
        account.push_back(vfs_user{0, "root", "root"});
        account.push_back(vfs_user{1, "cc", "cc"});
        current_user = 0;
        last_user = 1;
        root = new_node(fs_dir);
        pwd = "/";
        auto n = now();
        year = localtime(&n)->tm_year;
        current_user = 1;
        last_user = 0;
    }

    void cvfs::error(const string_t &str) {
        throw cexception(ex_vm, str);
    }

    static void mod_copy(vfs_mod *mod, const char *s) {
        for (int i = 0; i < 9; ++i) {
            ((char *) mod)[i] = *s++;
        }
    }

    vfs_node::ref cvfs::new_node(vfs_file_t type) {
        auto node = std::make_shared<vfs_node>();
        node->type = type;
        if (type == fs_file) {
            mod_copy(node->mod, "rw-r--r--");
        } else if (type == fs_dir) {
            mod_copy(node->mod, "rw-r--r--");
        } else {
            error("invalid mod");
        }
        time_t ctime;
        time(&ctime);
        node->time.create = ctime;
        node->time.access = ctime;
        node->time.modify = ctime;
        node->owner = current_user;
        node->refs = 0;
        node->locked = false;
        node->callback = nullptr;
        return node;
    }

    string_t cvfs::get_user() const {
        return account[current_user].name;
    }

    string_t cvfs::get_pwd() const {
        return pwd;
    }

    char* cvfs::file_time(const time_t &t) const {
        auto timeptr = localtime(&t);
        /*static const char wday_name[][4] = {
                "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
        };*/
        static const char mon_name[][4] = {
                "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
        };
        static char result[32];
        if (year == timeptr->tm_year) {
            sprintf(result, "%.3s%3d %.2d:%.2d",
                    mon_name[timeptr->tm_mon],
                    timeptr->tm_mday, timeptr->tm_hour,
                    timeptr->tm_min);
        } else {
            sprintf(result, "%.3s%3d %5d",
                    //wday_name[timeptr->tm_wday],
                    mon_name[timeptr->tm_mon],
                    timeptr->tm_mday,
                    1900 + timeptr->tm_year);
        }
        return result;
    }

    void cvfs::ll(const string_t &name, const vfs_node::ref &node, std::ostream &os) const {
        if (!node)
            return;
        static char fmt[256];
        sprintf(fmt, "\033FFFA0A0A0\033%c%9s \033FFFB3B920\033%4s \033S4\033%9d \033FFF51C2A8\033%s \033FFF35EA3F\033%s\033S4\033",
                node->type == fs_dir ? 'd' : '-',
                (char *) node->mod,
                account[node->owner].name.data(),
                node->data.size(),
                file_time(node->time.create),
                name.data());
        os << fmt << std::endl;
    }

    int cvfs::macro(const std::vector<string_t> &m, const vfs_node::ref &node, vfs_node_dec **dec) const {
        if (m[1] == "ls") {
            std::stringstream ss;
            std::transform(node->children.begin(), node->children.end(),
                           std::ostream_iterator<string_t>(ss, "\n"),
                           [](const auto &p) { return p.first; });
            auto str = ss.str();
            if (!str.empty())
                str.pop_back();
            *dec = new vfs_node_cached(str);
            return 0;
        }
        if (m[1] == "ll") {
            std::stringstream ss;
            ll("..", node->parent.lock(), ss); // parent
            ll(".", node, ss); // self
            for (auto &c : node->children) {
                ll(c.first, c.second, ss); // children
            }
            auto str = ss.str();
            if (!str.empty())
                str.pop_back();
            *dec = new vfs_node_cached(str);
            return 0;
        }
        return -2;
    }

    int cvfs::get(const string_t &path, vfs_node_dec **dec, vfs_func_t *f) const {
        std::vector<string_t> m;
        split_path(path, m, ':');
        auto p = combine(pwd, m[0]);
        auto node = get_node(p);
        if (!node)
            return -1;
        if (node->type == fs_file) {
            if (node->locked)
                return -3;
            node->time.access = now();
            if (dec)
                *dec = new vfs_node_solid(node);
            return 0;
        }
        if (node->type == fs_func) {
            node->time.access = now();
            if (dec) {
                if (f)
                    *dec = new vfs_node_cached(f->callback(p));
                else
                    return -2;
            }
            return 0;
        }
        if (node->type == fs_dir) {
            if (m.size() > 1) {
                return macro(m, node, dec);
            }
        }
        return -2;
    }

    bool cvfs::read_vfs(const string_t &path, std::vector<byte> &data) const {
        auto node = get_node(path);
        if (!node)
            return false;
        if (node->type != fs_file)
            return false;
        data.resize(node->data.size());
        std::copy(node->data.begin(), node->data.end(), data.begin());
        return true;
    }

    void cvfs::as_root(bool flag) {
        if (flag) {
            if (current_user != 0) {
                last_user = current_user;
                current_user = 0;
            }
        } else {
            if (current_user == 0) {
                current_user = last_user;
                last_user = 0;
            }
        }
    }

    bool cvfs::write_vfs(const string_t &path, const std::vector<byte> &data) {
        auto node = get_node(path);
        if (!node) {
            touch(path);
            node = get_node(path);
            if (!node)
                return false;
        }
        if (node->type != fs_file)
            return false;
        if (!node->data.empty())
            return false;
        node->data.resize(data.size());
        std::copy(data.begin(), data.end(), node->data.begin());
        return true;
    }

    string_t get_parent(const string_t &path) {
        assert(path[0] == '/');
        if (path == "/")
            return path;
        auto f = path.find_last_of('/');
        assert(f != string_t::npos);
        if (f == 0)
            return "/";
        return path.substr(0, f);
    }

    time_t cvfs::now() {
        time_t ctime;
        time(&ctime);
        return ctime;
    }

    void cvfs::split_path(const string_t &path, std::vector<string_t> &args, char c) {
        std::stringstream ss(path);
        string_t temp;
        while (std::getline(ss, temp, c)) {
            args.push_back(temp);
        }
    }

    vfs_node::ref cvfs::get_node(const string_t &path) const {
        std::vector<string_t> paths;
        split_path(path, paths, '/');
        auto cur = root;
        for (auto i = 0; i < paths.size(); ++i) {
            if (!can_mod(cur, 0))
                return nullptr;
            auto &p = paths[i];
            if (!p.empty()) {
                auto f = cur->children.find(p);
                if (f != cur->children.end()) {
                    if (i < paths.size() - 1 && f->second->type != fs_dir)
                        return nullptr;
                    cur = f->second;
                } else {
                    return nullptr;
                }
            }
        }
        return cur;
    }

    int cvfs::cd(const string_t &path) {
        auto p = combine(pwd, path);
        auto node = get_node(p);
        if (!node)
            return -1;
        switch (node->type) {
            case fs_file:
                return -2;
            case fs_dir:
                pwd = p;
                break;
            case fs_func:
                break;
        }
        return 0;
    }

    int cvfs::_mkdir(const string_t &path, vfs_node::ref &cur) {
        std::vector<string_t> paths;
        split_path(path, paths, '/');
        cur = root;
        bool update = false;
        for (auto &p : paths) {
            if (!p.empty()) {
                auto f = cur->children.find(p);
                if (f != cur->children.end()) {
                    cur = f->second;
                    if (f->second->type != fs_dir)
                        return -2;
                } else {
                    if (!update)
                        update = true;
                    auto node = new_node(fs_dir);
                    node->parent = cur;
                    cur->children.insert(std::make_pair(p, node));
                    cur = node;
                }
            }
        }
        if (update)
            return 0;
        return -1;
    }

    int cvfs::mkdir(const string_t &path) {
        auto p = combine(pwd, path);
        vfs_node::ref cur;
        return _mkdir(p, cur);
    }

    string_t cvfs::combine(const string_t &pwd, const string_t &path) const {
        if (path.empty())
            return pwd;
        if (path[0] == '/')
            return path;
        auto res = pwd;
        std::vector<string_t> paths;
        split_path(path, paths, '/');
        for (auto &p : paths) {
            if (!p.empty()) {
                if (p == ".")
                    continue;
                else if (p == "..")
                    res = get_parent(res);
                else if (res.back() == '/')
                    res += p;
                else
                    res += "/" + p;
            }
        }
        return res;
    }

    int cvfs::touch(const string_t &path) {
        auto p = combine(pwd, path);
        auto node = get_node(p);
        if (!node) {
            vfs_node::ref cur;
            auto s = _mkdir(p, cur);
            if (s == 0) { // new dir
                cur->type = fs_file;
                return -1;
            } else { // exists
                _touch(cur);
                return 0;
            }
        }
        switch (node->type) {
            case fs_file:
            case fs_dir:
                _touch(node);
                return 0;
            default:
                return -2;
        }
    }

    void cvfs::_touch(vfs_node::ref &node) {
        auto ctime = now();
        node->time.create = ctime;
        node->time.access = ctime;
        node->time.modify = ctime;
    }

    int cvfs::func(const string_t &path, vfs_func_t *f) {
        auto node = get_node(path);
        if (!node) {
            vfs_node::ref cur;
            auto s = _mkdir(path, cur);
            if (s == 0) { // new dir
                cur->type = fs_func;
                cur->callback = f;
                return 0;
            } else { // exists
                return 1;
            }
        }
        return -2;
    }

    string_t cvfs::get_filename(const string_t &path) {
        if (path.empty())
            return "";
        if (path == "/")
            return "";
        auto f = path.find_last_of('/');
        if (f == string_t::npos)
            return "";
        return path.substr(f + 1);
    }

    int cvfs::rm(const string_t &path) {
        auto p = combine(pwd, path);
        auto node = get_node(p);
        if (!node)
            return -1;
        return node->parent.lock()->children.erase(get_filename(path)) == 0 ?
               -2 : (node->type != fs_dir ? 0 : 1);
    }

    int cvfs::rm_safe(const string_t &path) {
        auto p = combine(pwd, path);
        auto node = get_node(p);
        if (!node)
            return -1;
        if (!can_rm(node))
            return -2;
        return node->parent.lock()->children.erase(get_filename(path)) == 0 ?
               -3 : (node->type != fs_dir ? 0 : 1);
    }

    bool cvfs::can_rm(const vfs_node::ref &node) const {
        if (!can_mod(node, 1))
            return false;
        if (node->refs > 0)
            return false;
        if (node->locked)
            return false;
        if (node->type == fs_dir) {
            for (auto &c : node->children) {
                if (!can_rm(c.second))
                    return false;
            }
        }
        return true;
    }

    bool cvfs::can_mod(const vfs_node::ref &node, int mod) const {
        if (mod != -1) {
            if (node->mod[0].rwx[mod] == '-')
                return false;
            if (node->owner != current_user) {
                if (node->mod[1].rwx[mod] == '-')
                    return false;
                if (node->mod[2].rwx[mod] == '-')
                    return false;
            }
        }
        return true;
    }
}
