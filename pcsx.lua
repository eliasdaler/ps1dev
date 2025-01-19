local addresses = {}
PCSX.execSlots[255] = function()
  local mem = PCSX.getMemPtr()
  local regs = PCSX.getRegisters().GPR.n
  local name = ffi.string(mem + bit.band(regs.a1, 0x7fffff))
  addresses[name] = regs.a0
end

function DrawImguiFrame()
  imgui.safe.Begin('Magic', true, function()
    local mem = PCSX.getMemoryAsFile()
    local addr = addresses['game.renderer.fogColor']
    if type(addr) == 'number' then
      local color = { r = mem:readU8At(addr + 0) / 255, g = mem:readU8At(addr + 1) / 255, b = mem:readU8At(addr + 2) / 255 }
      local modified, n = imgui.extra.ColorEdit3('Fog color', color)
      if modified then
        mem:writeU8At(n.r * 255, addr + 0)
        mem:writeU8At(n.g * 255, addr + 1)
        mem:writeU8At(n.b * 255, addr + 2)
      end
    end
  end)
end
