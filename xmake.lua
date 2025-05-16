add_rules("mode.debug", "mode.release")

target("concurrent_stl")
    set_kind("headeronly")
    add_headerfiles("concurrent_unordered_map.h")

for _, file in ipairs(os.files("tests/test_*.cpp")) do
     local name = path.basename(file)
     target(name)
         set_kind("binary")
         set_default(false)
         add_files("tests/" .. name .. ".cpp")
         add_tests("default")

end