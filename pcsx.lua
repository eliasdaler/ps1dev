local function crash_Checkbox(mem, address, name, value, original)
  address = bit.band(address, 0x1fffff)
  local pointer = mem + address
  pointer = ffi.cast('uint8_t*', pointer)
  local changed
  local check
  local tempvalue = pointer[0]
  if tempvalue == original then check = false end
  if tempvalue == value then check = true else check = false end
  changed, check = imgui.Checkbox(name, check)
  if check then pointer[0] = value else pointer[0] = original end
end

-- This function is local as to not pollute the global namespace.
local function doSliderInt(mem, address, name, min, max)
  -- Clamping the address to the actual memory space, essentially
  -- removing the upper bank address using a bitmask. The result
  -- will still be a normal 32-bits value.
  address = bit.band(address, 0x1fffff)
  -- Computing the FFI pointer to the actual uint32_t location.
  -- The result will be a new FFI pointer, directly into the emulator's
  -- memory space, hopefully within normal accessible bounds. The
  -- resulting type will be a cdata[uint8_t*].
  local pointer = mem + address
  -- Casting this pointer to a proper uint32_t pointer.
  pointer = ffi.cast('uint8_t*', pointer)
  -- Reading the value in memory
  local value = pointer[0]
  -- Drawing the ImGui slider
  local changed
  changed, value = imgui.SliderInt(name, value, min, max, '%d')
  -- The ImGui Lua binding will first return a boolean indicating
  -- if the user moved the slider. The second return value will be
  -- the new value of the slider if it changed. Therefore we can
  -- reassign the pointer accordingly.
  if changed then pointer[0] = value end
end

function DrawImguiFrame()
  imgui.safe.Begin('Crash Bandicoot Mods', function()
    --[[local mem = PCSX.getMemPtr()
    fogAddrR = PCSX.Assembler.Internals.resolveSymbol("fogR").address
    fogAddrG = PCSX.Assembler.Internals.resolveSymbol("fogG").address
    fogAddrB = PCSX.Assembler.Internals.resolveSymbol("fogB").address
    doSliderInt(mem, fogAddrR, 'Fog R', 0, 255)
    doSliderInt(mem, fogAddrG, 'Fog G', 0, 255)
    doSliderInt(mem, fogAddrB, 'Fog B', 0, 255)]]--
  end)
end

myGameSymbols = {}
PCSX.execSlots = {}
PCSX.execSlots[255] = function()
    print("HELLO")
    local mem = PCSX.getMemPtr()
    local regs = PCSX.getRegisters().GPR.n
    local address = regs.a0
    print(string.format("addr: %08x", address))
    local name = ffi.string(mem + bit.band(regs.a1, 0xffffff))
    myGameSymbols[name] = address
    return false
end
