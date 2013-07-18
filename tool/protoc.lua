
local args = {...}
if #args < 3 then
    print("usage: protoc.lua input_dir[.proto] define_dir pb_file")
    return -1
end

local lfs = require "lfs"
local plp = require "plp"

function getfiles(path, ext)
	local files = {}
	local init = -(#ext)
	local getattr = lfs.attributes
	for file in lfs.dir(path) do
		if file ~= "." and file ~= ".." then
			local fname = path .. '/' .. file
			if getattr(fname).mode == "file" and string.find(fname, ext, init, true) then
				files[#files+1] = fname
			end
		end	
	end
	return files
end

local files = getfiles(args[1], ".proto")

local full_blocktable = {}
local full_define = {}

local outdir = args[2]

for _, f in ipairs(files) do
    print(f)
    local blocktable = plp.block_new()
    assert(plp.parse_file(f, blocktable))
 
    local _, _, _, outfile = string.find(f,'(.-)([^\\/]*)$')
    local define = string.sub(outfile, 1, -7)
    outfile = string.format("%s/%s.h", outdir, define)
    local out = io.open(outfile, "w")
    plp.dump(blocktable, define, out) 
    out:close()

    for _, b in ipairs(blocktable) do
        full_blocktable[#full_blocktable+1] = b
    end
    full_define[#full_define+1] = define
end

local out = io.open(args[3], "w")
plp.dump_context(full_blocktable, full_define, out)
out:close()

