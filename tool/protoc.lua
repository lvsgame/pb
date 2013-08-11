require "pathconfig"
local args = {...}
if #args < 3 then
    print("usage: protoc.lua input_dir[.proto] out_dir pbinit_file")
    return 1
end

local fs = require "fs"
local plp = require "plp"

local files = fs.getfiles(args[1], ".proto")

local protos = {}
local outdir = args[2]
local pbinit_file = args[3]

for _, f in ipairs(files) do
    local _, _, _, protoname = string.find(f,'(.-)([^\\/]*).proto$')
    local proto = plp.parse_file(f, protoname)
 
    local outfile = string.format("%s/%s.pb.h", outdir, protoname)
    local out = io.open(outfile, "w")
    plp.dump_proto(proto, out) 
    out:close()

    protos[#protos+1] = proto
end

local _, _, outdir, outname = string.find(pbinit_file,'(.-)([^\\/]*).h$')
local tempfile = string.format("%s~%s.h", outdir, outname)
local out = io.open(tempfile, "w")
plp.dump_context(protos, out, outname)
out:close()
-- 输出到临时文件，保证pbinit_file生成后必定完整
os.rename(tempfile, pbinit_file)
return 0
