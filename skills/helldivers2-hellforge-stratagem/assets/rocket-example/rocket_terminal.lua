-- Camera ownership only. Collision, steering and damage stay on the game thread.
local S=assert(stingray)
local A,W,U,C,Q,V=S.Application,S.World,S.Unit,S.Camera,S.Quaternion,S.Vector3
local poll=assert(__liuli_rocket_terminal_poll)
local ack=assert(__liuli_rocket_terminal_ack)
local notice=assert(__liuli_optical_notice)
local clock=assert(__liuli_optical_poll)
local state={generation=0,targets={}}
local mine=setmetatable({},{__mode='k'})
local last_update,last_render=-10000,-10000
local warned=false
local busy=false
local unpack=unpack or table.unpack
local function call(fn,...)
 if type(fn)~='function' then return nil end
 local ok,a,b,c,d=pcall(fn,...)
 if ok then return a,b,c,d end
end
local function xyz(p)
 if not p then return nil end
 local ok,x,y,z=pcall(function() return V.x(p),V.y(p),V.z(p) end)
 if ok and x==x and y==y and z==z then return x,y,z end
end
local function cameras(world)
 local units=call(W.units_by_resource,world,'core/units/camera') or {}
 if #units==0 then
  units={}
  local all=call(W.units,world) or {}
  local first=state.scan or 1
  local last=math.min(#all,first+127)
  for i=first,last do
   local u=all[i]
   if call(U.alive,u) and (call(U.num_cameras,u) or 0)>0 then units[#units+1]=u end
  end
  state.scan=last<#all and last+1 or 1
 end
 local out={}
 for _,u in ipairs(units) do
  if call(U.alive,u) then
   for slot=1,math.min(4,call(U.num_cameras,u) or 1) do
    local cam=call(U.camera,u,slot)
    if cam then out[#out+1]={u=u,slot=slot,cam=cam,key=tostring(u)..'#'..slot} end
   end
  end
 end
 return out
end
local function restore(reason)
 if state.world and state.world==call(A.main_world) then
  for _,home in pairs(state.targets) do
   local it=home.camera
   if it and call(U.alive,it.u) then
    local cam=call(U.camera,it.u,it.slot)
    if cam then
     call(C.set_local_position,cam,it.u,V(home.x,home.y,home.z))
     if home.q and Q.from_elements then
      local q=Q.from_elements(unpack(home.q))
      call(C.set_local_rotation,cam,it.u,q)
     end
     if home.fov then call(C.set_vertical_fov,cam,home.fov) end
     call(W.update_unit,state.world,it.u)
    end
   end
  end
 end
 if state.generation~=0 then notice('TV view '..state.generation..': released ('..(reason or 'control ended')..')') end
 state={generation=0,targets={}}
end
local function frame(where)
 local now=clock()
 local generation,x,y,z,fx,fy,fz,age=poll()
 local world=call(A.main_world)
 if generation==0 or not world then
  if state.generation~=0 then restore() end
  return
 end
 if state.generation~=generation or state.world~=world then
  restore()
  state={generation=generation,world=world,targets={},started=now}
 end
 -- The projectile must first publish a real pose, not the zeroed launch snapshot.
 if not age or age<0.04 then return end
 for _,value in ipairs({x,y,z,fx,fy,fz}) do
  if type(value)~='number' or value~=value or math.abs(value)>100000 then
   ack(generation,0);restore();return
  end
 end
 if not fz or fx*fx+fy*fy+fz*fz<0.5 then ack(generation,0);restore();return end
 -- Refresh discovery at 4 Hz; only the cached lenses move at frame rate.
 if not state.list or now>=(state.next_scan or 0) then
  state.list=cameras(world)
  state.next_scan=now+250
 end
 local list=state.list
 -- Unit.camera can change while the owning unit stays alive. Do not cache its handle.
 for _,it in ipairs(list) do
  it.cam=call(U.alive,it.u) and call(U.camera,it.u,it.slot) or nil
 end
 local _,_,_,_,_,avatar=__liuli_rockets_poll()
 local ax,ay,az
 if avatar then ax,ay,az=xyz(call(U.world_position,avatar,1)) end
 local nearest,best
 for _,it in ipairs(list) do
  if it.cam and not state.targets[it.key] then
   local px,py,pz=xyz(call(C.local_position,it.cam))
   if px then
    local distance=ax and ((px-ax)^2+(py-ay)^2+(pz-az)^2) or math.huge
    if not best or distance<best then nearest,best=it,distance end
    if distance<20^2 then
     local q=call(C.local_rotation,it.cam)
     local qa,qb,qc,qd=call(Q.to_elements,q)
     state.targets[it.key]={camera=it,x=px,y=py,z=pz,q=qa and {qa,qb,qc,qd},fov=call(C.vertical_fov,it.cam)}
    end
   end
  end
 end
 if not next(state.targets) and nearest and (#list==1 or (best and best<30^2)) then
  local px,py,pz=xyz(call(C.local_position,nearest.cam))
  local qa,qb,qc,qd=call(Q.to_elements,call(C.local_rotation,nearest.cam))
  state.targets[nearest.key]={camera=nearest,x=px,y=py,z=pz,q=qa and {qa,qb,qc,qd},fov=call(C.vertical_fov,nearest.cam)}
 end
 local q=Q.look(V(fx,fy,fz),V(0,0,1))
 local touched=0
 local updated={}
 for _,it in ipairs(list) do
  if state.targets[it.key] and it.cam then
   -- Put the lens just ahead of the missile mesh, not inside its exhaust/model.
   C.set_local_position(it.cam,it.u,V(x+fx*1.1,y+fy*1.1,z+fz*1.1))
   C.set_local_rotation(it.cam,it.u,q)
   C.set_vertical_fov(it.cam,math.rad(62))
   updated[it.u]=true
   touched=touched+1
  end
 end
 for u in pairs(updated) do W.update_unit(world,u) end
 if touched>0 and where=='render' then
  ack(generation,1)
  if not state.confirmed then
   state.confirmed=true;notice('TV view '..generation..': render confirmed, '..touched..' lens(es)')
  end
 elseif touched==0 and now-state.started>1500 then
  notice('TV view '..generation..': no camera, '..#list..' candidate(s)')
  ack(generation,0);restore('no camera')
 end
end
local function guarded(where)
 if where=='render' then last_render=clock() else last_update=clock() end
 if busy then return end
 busy=true
 local ok,why=pcall(frame,where)
 if not ok then
  if state.generation~=0 then ack(state.generation,0);pcall(restore) end
  if not warned then warned=true;notice('TV camera: '..tostring(why)) end
 end
 busy=false
end
local function pack(...) return {n=select('#',...),...} end
local function wrap()
 local original=rawget(_G,'update')
 if type(original)=='function' and not mine[original] then
  local wrapped=function(...)
   local out=pack(original(...));guarded('update');return unpack(out,1,out.n)
  end
  mine[wrapped]=true;rawset(_G,'update',wrapped)
 end
 local render=rawget(_G,'render')
 if type(render)=='function' and not mine[render] then
  local wrapped=function(...)
   guarded('render');return render(...)
  end
  mine[wrapped]=true;rawset(_G,'render',wrapped)
 end
end
local check=assert(__liuli_optical_check)
__liuli_optical_check=function()
 check()
 local now=clock()
 if now-last_update>350 or now-last_render>350 then wrap() end
end
wrap()
notice('Rockets: TV late-render ownership, fresh camera handles and capture diagnostics installed')
