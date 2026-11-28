-- Presentation only. Native code owns ammunition, lock rules and projectiles.
local S=assert(stingray)
local A,W,U,G,V,M=S.Application,S.World,S.Unit,S.Gui,S.Vector3,S.Matrix4x4
local poll=assert(__liuli_rockets_poll)
local observe=assert(__liuli_rockets_observe)
local notice=assert(__liuli_optical_notice)
local special_target=rawget(_G,'__liuli_rocket_special_target')
local terminal_poll=rawget(_G,'__liuli_rocket_terminal_poll')
local gui,world,main,world_count
local ids,shapes={},{}
local next_frame,last_frame,used=0,-10000,0
local screen_camera,marker_x,marker_y,marker_size
local screen_origin,screen_forward
local mine=setmetatable({},{__mode='k'})
local warned={}
local unpack=unpack or table.unpack
local glyphs={
 ['0']='111101101101111',['1']='010110010010111',['2']='111001111100111',
 ['3']='111001111001111',['4']='101101111001001',['5']='111100111001111',
 ['6']='111100111101111',['7']='111001001001001',['8']='111101111101111',
 ['9']='111101111001111',['A']='010101111101101',['B']='110101110101110',
 ['C']='111100100100111',['D']='110101101101110',['E']='111100110100111',
 ['F']='111100110100100',['G']='111100101101111',['H']='101101111101101',
 ['I']='111010010010111',['J']='001001001101111',['K']='101101110101101',
 ['L']='100100100100111',['M']='101111111101101',['N']='101111111111101',
 ['O']='111101101101111',['P']='110101110100100',['Q']='111101101111001',
 ['R']='110101110101101',['S']='111100111001111',['T']='111010010010010',
 ['U']='101101101101111',['V']='101101101101010',['W']='101101111111101',
 ['X']='101101010101101',['Y']='101101010010010',['Z']='111001010100111',
 ['/']='001001010100100',['-']='000000111000000',[':']='000010000010000',
 ['[']='110100100100110',[']']='011001001001011'
}
-- BEGIN GENERATED CJK GLYPHS
local chinese={} -- Generate with a locally licensed font before use.
-- END GENERATED CJK GLYPHS
-- Names verified on the original enemy skeletons; absent nodes are never used.
local function report(tag,why)
 if not warned[tag] then warned[tag]=true;notice('Rockets '..tag..': '..tostring(why)) end
end
local function reset(destroy)
 if destroy and gui and world then
  for _,w in ipairs(A.worlds() or {}) do if w==world then W.destroy_gui(world,gui);break end end
 end
 gui,world,main,world_count=nil,nil,nil,nil
 ids,shapes={},{}
end
local function rect(x,y,w,h,a,r,g,b)
 used=used+1
 local old=shapes[used]
 if old and old[1]==x and old[2]==y and old[3]==w and old[4]==h and old[5]==a
  and old[6]==r and old[7]==g and old[8]==b then return end
 local p,size,color=V(x,y,-21),S.Vector2(w,h),S.Color(a,r,g,b)
 if ids[used] then G.update_rect(gui,ids[used],p,size,color)
 else ids[used]=G.rect(gui,p,size,color) end
 shapes[used]={x,y,w,h,a,r,g,b}
end
local function label(text,x,y,k,r,g,b)
 local offset=0
 for c in text:gmatch('[%z\1-\127\194-\244][\128-\191]*') do
  local glyph=glyphs[c]
  if glyph then for bit=1,15 do if glyph:sub(bit,bit)=='1' then
   rect(x+(offset+((bit-1)%3)*2)*k,y+(4-math.floor((bit-1)/3))*2*k,
    1.6*k,1.6*k,245,r,g,b)
  end end end
  local rows=chinese[c]
  if rows then for row=0,15 do
   local bits=tonumber(rows:sub(row*4+1,row*4+4),16)
   for col=0,15 do if math.floor(bits/2^(15-col))%2==1 then
    rect(x+(offset+col)*k,y+(13-row)*k,0.9*k,0.9*k,245,r,g,b)
   end end
  end end
  offset=offset+(rows and 17 or 8)
 end
end
local function key_name(key)
 if key>=112 and key<=123 then return 'F'..(key-111) end
 if key>=48 and key<=90 then return string.char(key) end
 return tostring(key)
end
local function draw(state,count,distance,keys,progress)
 local kind=state%4
 if math.floor(state/1024)%2==1 then kind=3
 elseif math.floor(state/2048)%2==1 then kind=4 end
 if kind==0 then reset(true);return end
 local current=A.main_world()
 if not current then reset(false);return end
 local list,overlay=A.worlds() or {},nil
 for _,w in ipairs(list) do if w~=current then overlay=w;break end end
 if not overlay then reset(false);return end
 if gui and (main~=current or world~=overlay or world_count~=#list) then reset(false) end
 if not gui then
  gui=assert(W.create_screen_gui(overlay,'scale',1,1),'no UI world')
  world,main,world_count=overlay,current,#list
  if G.set_visible then G.set_visible(gui,true) end
 end
 local width,height=A.back_buffer_size()
 if not width or not height or height<240 then return end
 local k=height/1080
 local x,y=370*k,height-135*k
 local guided=math.floor(state/4)%2==1
 local mode=kind==1 and 0 or math.floor(state/8)%4
 local locked=math.floor(state/64)%2==1
 local candidate=math.floor(state/128)%2==1
 local reloading=math.floor(state/256)%2==1
 local ready=math.floor(state/512)%2==1
 local controlling=math.floor(state/4096)%2==1
 local modes={'单发','齐射','集中','空爆'}
 local mag,reserve=count%256,math.floor(count/256)%256
 local r,g,b=locked and 111 or 255,locked and 225 or 222,locked and 167 or 90
 used=0
 -- Spear-style acquisition brackets follow the actual target/weak-point,
 -- not a fixed mark at the centre of the display.
 if guided then
  local cx,cy=width/2,height/2
  local size=72*k
  for _,sx in ipairs({-1,1}) do for _,sy in ipairs({-1,1}) do
   local px,py=cx+sx*size,cy+sy*size
   rect(px-(sx>0 and 16*k or 0),py,16*k,1*k,155,210,217,190)
   rect(px,py-(sy>0 and 16*k or 0),1*k,16*k,155,210,217,190)
  end end
  if candidate and marker_x and marker_y then
   local s=(marker_size or 28)*k
   local mx,my=marker_x,marker_y
   for _,sx in ipairs({-1,1}) do for _,sy in ipairs({-1,1}) do
    local px,py=mx+sx*s,my+sy*s
    rect(px-(sx>0 and 12*k or 0),py,12*k,2*k,245,r,g,b)
    rect(px,py-(sy>0 and 12*k or 0),2*k,12*k,245,r,g,b)
   end end
   rect(mx-5*k,my,10*k,1*k,245,r,g,b)
   rect(mx,my-5*k,1*k,10*k,245,r,g,b)
   rect(mx-24*k,my-s-8*k,48*k*math.max(0,math.min(progress or (locked and 1 or 0),1)),2*k,245,r,g,b)
   label(math.floor(distance+0.5)..'米',mx+s+8*k,my,k,r,g,b)
  end
 end
 if kind==4 and special_target and screen_camera and S.Camera.world_to_screen then
  for i=1,6 do
   local assigned,px,py,pz=special_target(i)
   local front=screen_origin and screen_forward and px and
    (px-screen_origin[1])*screen_forward[1]+(py-screen_origin[2])*screen_forward[2]+(pz-screen_origin[3])*screen_forward[3]>0.5
   if assigned and assigned>0 and front then
    local p=S.Camera.world_to_screen(screen_camera,V(px,py,pz))
    local mx,my=V.x(p),V.y(p)
    if mx==mx and my==my and mx>0 and mx<width and my>0 and my<height then
     local s=23*k
     for _,sx in ipairs({-1,1}) do for _,sy in ipairs({-1,1}) do
      rect(mx+sx*s-(sx>0 and 9*k or 0),my+sy*s,9*k,k,225,111,225,167)
      rect(mx+sx*s,my+sy*s-(sy>0 and 9*k or 0),k,9*k,225,111,225,167)
     end end
     label('X'..assigned,mx+s+5*k,my,k,111,225,167)
    end
   end
  end
 end
 if controlling then
  local cx,cy=width/2,height/2
  rect(cx-18*k,cy,12*k,k,245,111,225,167);rect(cx+6*k,cy,12*k,k,245,111,225,167)
  rect(cx,cy-18*k,k,12*k,245,111,225,167);rect(cx,cy+6*k,k,12*k,245,111,225,167)
  for _,sx in ipairs({-1,1}) do for _,sy in ipairs({-1,1}) do
   local px,py=width/2+sx*width*0.4,height/2+sy*height*0.36
   rect(px-(sx>0 and 34*k or 0),py,34*k,2*k,155,111,225,167)
   rect(px,py-(sy>0 and 34*k or 0),2*k,34*k,155,111,225,167)
  end end
 end
 rect(x-10*k,y-34*k,392*k,132*k,175,12,15,18)
 rect(x-10*k,y-34*k,2*k,132*k,240,r,g,b)
 local titles={'次式实验性火箭筒','智能火箭筒','数控火箭筒','天穹智能导弹系统'}
 label(titles[kind],x,y+76*k,k,232,235,221)
 label(kind==3 and (controlling and '弹载视角 / 手动操纵' or '数控反坦克 / 电视制导') or
  kind==4 and (guided and '锁定追踪 / 指定目标' or '直射升空 / 自主分配目标') or
  (guided and '锁定' or '直射')..' / '..modes[mode+1],x,y+54*k,k,r,g,b)
 local status=not ready and '武器初始化中' or reloading and '正在装填' or
  not guided and '直射就绪' or locked and ('已锁定 '..math.floor(distance+0.5)..'米') or
  candidate and '正在锁定' or '瞄准中型或大型目标'
 if ready and not reloading and kind==3 then status=controlling and
  (candidate and '目标可交接 / 鼠标操纵' or '电视制导 / 鼠标操纵') or '发射后进入弹载视角' end
 if ready and not reloading and kind==4 then status=guided and
  (locked and '已锁定 / 发射后持续追踪' or candidate and '正在锁定' or '瞄准目标以锁定') or
  '无需锁定 / 在空导弹 '..(special_target and special_target(0) or 0) end
 label(status,x,y+32*k,k,r,g,b)
 label('弹药 '..mag..(kind~=1 and '/'..reserve or '/4'),x,y+10*k,k,232,235,221)
 if kind==2 then
  label('['..key_name(keys%256)..']锁定/直射 ['..key_name(math.floor(keys/256)%256)..']弹头',
   x,y-12*k,k,176,184,185)
  label('['..key_name(math.floor(keys/65536)%256)..']装填',x+186*k,y+10*k,k,176,184,185)
 elseif kind==3 then
  local exit_key,speed,boosted=121,30,false
  if terminal_poll then
   local values={terminal_poll()};exit_key=values[9] or exit_key
   speed,boosted=values[11] or speed,values[12]==true
  end
  label('['..key_name(keys%256)..']锁定交接 ['..key_name(exit_key or 121)..']返回',x,y-12*k,k,176,184,185)
  if controlling then
   label((boosted and '高速 ' or '低速 ')..math.floor(speed+0.5)..'米/秒',x+186*k,y+10*k,k,r,g,b)
   label('[SHIFT]切换',x+262*k,y-12*k,k,176,184,185)
  else
   label('['..key_name(math.floor(keys/65536)%256)..']装填',x+186*k,y+10*k,k,176,184,185)
  end
 elseif kind==4 then
  label('['..key_name(keys%256)..']锁定/直射',x,y-12*k,k,176,184,185)
  label('['..key_name(math.floor(keys/65536)%256)..']装填',x+186*k,y+10*k,k,176,184,185)
 else
  label('锁定后发射 / 发射后无需持续瞄准',x,y-12*k,k,176,184,185)
 end
 for i=used+1,#ids do
  if shapes[i] and shapes[i][5]~=0 then
   G.update_rect(gui,ids[i],V(0,0,-21),S.Vector2(0,0),S.Color(0,0,0,0))
   shapes[i]={0,0,0,0,0,0,0,0}
  end
 end
end
local function camera(target,avatar,tx,ty,tz)
 marker_x,marker_y=nil,nil
 screen_camera=nil
 screen_origin,screen_forward=nil,nil
 local current=A.main_world()
 if not current then return end
 local origin,forward,best
 -- Use the player's real camera. debug_camera_pose can be detached or stale.
 if S.Camera and S.Quaternion and W.units_by_resource and U.camera then
  local units=W.units_by_resource(current,'core/units/camera') or {}
  local home=avatar and U.alive(avatar) and U.world_position(avatar,1)
  for _,u in ipairs(units) do
   local ok,p,f,c=pcall(function()
    local cam=U.camera(u,1)
    return S.Camera.local_position(cam),S.Quaternion.forward(S.Camera.local_rotation(cam)),cam
   end)
   if ok and p and f then
    local d=home and ((V.x(p)-V.x(home))^2+(V.y(p)-V.y(home))^2+(V.z(p)-V.z(home))^2) or 0
    if not best or d<best then best=d;origin=p;forward=f;screen_camera=c end
  end
  end
 end
 if not origin or not forward then
  local pose=W.debug_camera_pose(current)
  if not pose then return end
  origin,forward=M.translation(pose),M.forward(pose)
 end
 local ox,oy,oz=V.x(origin),V.y(origin),V.z(origin)
 local fx,fy,fz=V.x(forward),V.y(forward),V.z(forward)
 screen_origin,screen_forward={ox,oy,oz},{fx,fy,fz}
 observe(ox,oy,oz,fx,fy,fz)
 if target and U.alive(target) then
  -- The native lock supplies its selected targeting node, including buildings.
  local point=tx and V(tx,ty,tz) or U.world_position(target,1)
  local dx,dy,dz=V.x(point)-ox,V.y(point)-oy,V.z(point)-oz
  if dx*fx+dy*fy+dz*fz>0.5 and screen_camera and S.Camera.world_to_screen then
   local projected=S.Camera.world_to_screen(screen_camera,point)
   marker_x,marker_y=V.x(projected),V.y(projected)
   local width,height=A.back_buffer_size()
   if marker_x~=marker_x or marker_y~=marker_y or marker_x<0 or marker_x>width or marker_y<0 or marker_y>height then
    marker_x,marker_y=nil,nil
   end
   marker_size=math.max(18,math.min(52,2200/math.max(40,math.sqrt(dx*dx+dy*dy+dz*dz))))
  elseif dx*fx+dy*fy+dz*fz>0.5 then
   local shared=rawget(_G,'chef')
   if shared and type(shared.world_to_screen)=='function' then
    marker_x,marker_y=shared.world_to_screen(V.x(point),V.y(point),V.z(point))
   end
  end
 end
end
local function frame()
 local now=__liuli_optical_poll()
 last_frame=now
 if now<next_frame then return end
 next_frame=now+100
 local state,count,target,distance,keys,avatar,tx,ty,tz,progress=poll()
 local ok,why=pcall(camera,target,avatar,tx,ty,tz)
 if not ok then report('camera',why) end
 ok,why=pcall(draw,state,count,distance,keys,progress)
 if not ok then report('HUD',why) end
end
local function pack(...) return {n=select('#',...),...} end
local function wrap()
 local original=rawget(_G,'update')
 if type(original)=='function' and not mine[original] then
  local wrapped=function(...)
   local out=pack(original(...))
   local ok,why=pcall(frame)
   if not ok then report('frame',why) end
   return unpack(out,1,out.n)
  end
  mine[wrapped]=true;rawset(_G,'update',wrapped)
 end
end
local optical_check=assert(__liuli_optical_check)
__liuli_optical_check=function()
 optical_check()
 local now=__liuli_optical_poll()
 if now-last_frame>1500 then wrap() end
end
wrap()
notice('Rockets: independent guidance/warhead controls, weak-point bridge and pixel HUD installed')
