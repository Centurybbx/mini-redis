#include "internal.h"

namespace miniredis {
    class RObj_Str : public RObj {
        std::string s_;
    public:
        RObj_Str(const std::string& s) : s_(s) {}

        const std::string& Str() const {
            return s_;
        }

    };

    void ProcCmdGet(const std::vector<std::string>& args, std::string& resp) {
        if (args.size() != 2) {
            resp = "-ERR cmd arg count error\r\n";
            return;
        }

        RObj::Ptr robj = db::GetRObj(args.at(1));
        if (robj) {
            auto robj_s = dynamic_cast<RObj_Str*>(robj.get());
            if (robj_s) {
                resp = Sprintf("$%zu\r\n", robj_s->Str().size());
                resp.append(robj_s->Str());
                resp.append("\r\n");
            } else {
                resp = "-WRONGTYPE robj is not string for GET cmd\r\n";
            }
        } else {
            resp = "$-1\r\n";
        }
    }

    void ProcCmdSet(const std::vector<std::string>& args, std::string& resp) {
        if (args.size() != 3) {
            resp = "-ERR cmd arg count error\r\n";
            return;
        }

        db::SetRObj(args.at(1), RObj::Ptr(new RObj_Str(args.at(2))));
        resp += "+OK\r\n";
    }
}