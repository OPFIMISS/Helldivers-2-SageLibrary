-- Native SDK 16 owns energy, input and perception; this file owns presentation.
local S = assert(stingray)
local A, W, U, G, M = S.Application, S.World, S.Unit, S.Gui, S.Mesh
local poll = assert(__liuli_optical_poll)
local relation = assert(__liuli_optical_relation)
local notice = assert(__liuli_optical_notice)
local unpack = unpack or table.unpack
local world, gui, main, world_count, ids, shapes
local cache, key, next_scan, next_ui = {}, nil, 0, 0
local frame_world, any_cloak
local last_frame, render_wrapped = -10000, false
local reported, mine = {}, setmetatable({}, {__mode='k'})
local digits = {
  ['0']='111101101101111', ['1']='010110010010111',
  ['2']='111001111100111', ['3']='111001111001111',
  ['4']='101101111001001', ['5']='111100111001111',
  ['6']='111100111101111', ['7']='111001001001001',
  ['8']='111101111101111', ['9']='111101111001111', ['%']='101001010100101'
}
local function report(tag, why)
  if not reported[tag] then reported[tag]=true; notice(tag..': '..tostring(why)) end
end
local function worlds()
  return A.worlds() or {}
end
local function contains(list, value)
  for _, v in ipairs(list) do if v == value then return true end end
  return false
end
local function drop_gui(destroy)
  if destroy and gui and contains(worlds(), world) then W.destroy_gui(world, gui) end
  world, gui, ids, shapes, main, world_count = nil, nil, nil, nil, nil, nil
end
local function rect(i, x, y, width, height, alpha, red, green, blue)
  local old = shapes[i]
  if old and old[1]==x and old[2]==y and old[3]==width and old[4]==height
    and old[5]==alpha and old[6]==red and old[7]==green and old[8]==blue then return end
  local pos, size, color = S.Vector3(x,y,-20), S.Vector2(width,height), S.Color(alpha,red,green,blue)
  if ids[i] then G.update_rect(gui,ids[i],pos,size,color)
  else ids[i]=G.rect(gui,pos,size,color) end
  shapes[i]={x,y,width,height,alpha,red,green,blue}
end
local function energy_hud(charge, active)
  if charge < 0 then drop_gui(true); return end
  local current_main, list, overlay = A.main_world(), worlds(), nil
  if not current_main then drop_gui(false); return end
  -- Match NarrowHUD/Chef's actual HUD world, including below-native render scale.
  for _, candidate in ipairs(list) do if candidate ~= current_main then overlay=candidate; break end end
  if not overlay then drop_gui(false); return end
  if gui and (world ~= overlay or main ~= current_main or world_count ~= #list) then
    -- A reused world pointer is not proof that an old GUI still exists.
    drop_gui(false)
  end
  if not gui then
    gui = assert(W.create_screen_gui(overlay,'scale',1,1), 'cannot create overlay GUI')
    world, main, world_count, ids, shapes = overlay, current_main, #list, {}, {}
    if G.set_visible then G.set_visible(gui,true) end
    notice('persistent energy HUD created in UI world, layer -20')
  end
  local width, height = A.back_buffer_size()
  assert(type(width)=='number' and type(height)=='number' and height>=240,'invalid screen size')
  local k = height/1080
  rect(1,46*k,160*k,128*k,8*k,230,17,21,25)
  rect(2,47*k,161*k,math.max(1,126*k*charge),6*k,charge>0 and 255 or 0,
    active and 105 or (charge>=0.3 and 255 or 160), active and 208 or (charge>=0.3 and 225 or 173),
    active and 221 or 80)
  rect(3,(47+126*0.3)*k,160*k,k,8*k,235,190,197,190)
  local label=string.format('%3d%%',math.floor(charge*100+0.5))
  for c=1,4 do
    local pixels=digits[label:sub(c,c)] or '000000000000000'
    for p=1,15 do
      rect(3+(c-1)*15+p,(180+(c-1)*8+((p-1)%3)*2)*k,
        (159+(4-math.floor((p-1)/3))*2)*k,1.6*k,1.6*k,
        pixels:sub(p,p)=='1' and 255 or 0,230,233,219)
    end
  end
end

local function alive(u)
  return U.alive(u)
end
local function restore(u, entry)
  if entry.hidden and alive(u) then
    local n=U.num_meshes(u)
    for i, visible in ipairs(entry.saved) do
      if i<=n then U.set_mesh_visibility(u,i,visible) end
    end
  end
  entry.hidden=false
end
local function snapshot(u, entry)
  local n=U.num_meshes(u)
  assert(n>=0 and n<1024,'invalid character mesh count')
  for i=1,n do
    if entry.saved[i]==nil then entry.saved[i]=M.visibility(U.mesh(u,i)) end
  end
  return n
end
local function collect_meshes(current_main, stamp)
  if key==stamp then return end
  key=stamp
  if not (U.num_meshes and U.mesh and U.set_mesh_visibility and M and M.visibility) then
    report('meshes','mesh visibility API missing'); return
  end
  local candidates=W.units(current_main) or {}
  local total, units = 0, 0
  -- One scan when equipment/cloak/link identity changes, never a scan every frame.
  for _, u in ipairs(candidates) do
    if alive(u) then
      local hidden, tracked=relation(u)
      if tracked then
        local entry=cache[u] or {saved={},hidden=false}
        cache[u]=entry
        total=total+snapshot(u,entry); units=units+1
      end
    end
  end
  notice(string.format('character cache: %d units, %d meshes',units,total))
end
local function sync_meshes()
  for u, entry in pairs(cache) do
    if not alive(u) then cache[u]=nil
    else
      local hidden, tracked=relation(u)
      if not hidden then restore(u,entry) end
      if not tracked then cache[u]=nil end
    end
  end
end
local function hide_meshes()
  for u, entry in pairs(cache) do
    if alive(u) then
      local hidden=relation(u)
      if hidden then
        local n=snapshot(u,entry)
        for i=1,n do
          -- Reacquire native mesh objects in the current frame; keep only booleans.
          if M.visibility(U.mesh(u,i)) then U.set_mesh_visibility(u,i,false) end
        end
        entry.hidden=true
      elseif entry.hidden then restore(u,entry) end
    end
  end
end
local function frame()
  local now, charge, active, equipped, r0,r1,r2,r3,r4, revision=poll()
  last_frame=now
  local current=A.main_world()
  if current~=frame_world then
    cache, key = {}, nil
    frame_world=current
  end
  local has_cloak=r0~=0 or r1~=0 or r2~=0 or r3~=0 or r4~=0
  if now>=next_ui and current and (equipped~=0 or has_cloak) then
    local stamp=table.concat({equipped,r0,r1,r2,r3,r4,revision},':')
    if stamp~=key and now>=next_scan then
      local ok,why=pcall(collect_meshes,current,stamp)
      if not ok then report('mesh cache',why) end
      next_scan=now+250
    end
  end
  any_cloak=has_cloak
  local ok,why
  if not render_wrapped and has_cloak then
    ok,why=pcall(hide_meshes)
    if not ok then report('mesh hide',why) end
  end
  if now>=next_ui then
    next_ui=now+100
    ok,why=pcall(sync_meshes)
    if not ok then report('mesh restore',why) end
    ok,why=pcall(energy_hud,charge,active)
    if not ok then report('HUD',why) end
  end
end
local function pack(...) return {n=select('#',...),...} end
local function wrap()
  local update=rawget(_G,'update')
  if type(update)=='function' and not mine[update] then
    local wrapped=function(...)
      local out=pack(update(...))
      local ok,why=pcall(frame)
      if not ok then report('frame',why) end
      return unpack(out,1,out.n)
    end
    mine[wrapped]=true; rawset(_G,'update',wrapped)
  end
  local render=rawget(_G,'render')
  if type(render)=='function' and not mine[render] then
    local wrapped=function(...)
      if any_cloak then
        local ok,why=pcall(hide_meshes)
        if not ok then report('mesh hide',why) end
      end
      return render(...)
    end
    mine[wrapped]=true; rawset(_G,'render',wrapped); render_wrapped=true
  end
end
__liuli_optical_check=function()
  local now=poll()
  -- Other mods may wrap ours. Rewrap only when frames have stopped, avoiding
  -- an ever-growing chain of wrappers fighting over the outermost position.
  if now-last_frame>1500 then wrap() end
end
wrap()
notice('Lua presentation installed; HUD 10 Hz, mesh cache on identity changes')
