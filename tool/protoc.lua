require "pathconfig"
local args = {...}
if #args < 3 then
    print("usage: protoc.lua input_dir[.proto] define_dir pb_file")
    return 1
end

local fs = require "fs"
local plp = require "plp"

local files = fs.getfiles(args[1], ".proto")

local full_blocktable = {}
local full_define = {}

local outdir = args[2]

for _, f in ipairs(files) do
    local blocktable = plp.block_new()
    assert(plp.parse_file(f, blocktable))
 
    local _, _, _, define = string.find(f,'(.-)([^\\/]*).proto$')
    outfile = string.format("%s/%s.pb.h", outdir, define)
    local out = io.open(outfile, "w")
    plp.dump(blocktable, define, out) 
    out:close()

    for _, b in ipairs(blocktable) do
        full_blocktable[#full_blocktable+1] = b
    end
    full_define[#full_define+1] = define
end

local outfile = args[3]
local _, _, outdir, outname = string.find(outfile,'(.-)([^\\/]*).h$')
local tempfile = string.format("%s~%s.h", outdir, outname)
local out = io.open(tempfile, "w")
plp.dump_context(full_blocktable, full_define, out, outname)
out:close()
-- 输出到临时文件，保证outfile生成后必定完整
os.rename(tempfile, outfile)
return 0
