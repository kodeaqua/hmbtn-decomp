// Original PS1 command semantics, implemented for independent output pixels.
// No game logic, native RAM or native VRAM is written by this kernel.
Texture2DArray<uint> vram_cache : register(t0, space0);
ByteAddressBuffer scene : register(t1, space0);
RWTexture2D<float4> output_image : register(u0, space1);
#define HM_SCENE_GROUP_PRIMITIVE_CAPACITY 16
#define HM_SCENE_PRIMITIVE_STRIDE 41
groupshared uint hm_group_tile_valid;
groupshared uint hm_group_cache_enabled;
groupshared uint hm_group_bin;
groupshared uint hm_group_count;
groupshared uint hm_group_primitive_ids[HM_SCENE_GROUP_PRIMITIVE_CAPACITY];
groupshared uint hm_group_primitive_words[HM_SCENE_GROUP_PRIMITIVE_CAPACITY*HM_SCENE_PRIMITIVE_STRIDE];
uint load_word(uint index){return scene.Load(index*4);}
uint word(uint p,uint i){uint base=p&0x7fffffffu;
    return (p&0x80000000u)?hm_group_primitive_words[base+i]:load_word(base+i);}
uint vram(uint layer,int x,int y){uint2 at=uint2(uint(x)&1023,uint(y)&511);
    uint packed=vram_cache.Load(int4(int2(at.x>>1,at.y),int(layer),0));
    return (packed>>((at.x&1)*16))&65535;}
int edge_i(int2 a,int2 b,int2 p){return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);}
float edge_f(float2 a,float2 b,float2 p){return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);}
// Exact Q8 edge sign near a floating-point cancellation boundary. Emulated
// signed 64-bit products keep shader-int64 an optional device capability.
uint2 hm_mul_u64(uint a,uint b){uint a0=a&65535,a1=a>>16,b0=b&65535,b1=b>>16;
    uint low=a0*b0,middle=a1*b0+(low>>16),high=a1*b1+(middle>>16);
    middle=(middle&65535)+a0*b1;high+=middle>>16;
    return uint2((middle<<16)|(low&65535),high);}
uint2 hm_mul_s64(int a,int b){uint2 p=hm_mul_u64(uint(abs(a)),uint(abs(b)));
    if((a<0)!=(b<0)){p=~p;p.x+=1;if(p.x==0)p.y+=1;}return p;}
int hm_edge_sign(int2 a,int2 b,int2 p,float approximate){
    if(abs(approximate)>64.0)return approximate<0?-1:1;
    uint2 left=hm_mul_s64(b.x-a.x,p.y-a.y),right=hm_mul_s64(b.y-a.y,p.x-a.x);
    uint2 result=uint2(left.x-right.x,left.y-right.y-uint(left.x<right.x));
    return int(result.y)<0?-1:(any(result!=0)?1:0);}
bool top_left(int2 a,int2 b){return b.y<a.y||(b.y==a.y&&b.x>a.x);}
bool top_left_f(float2 a,float2 b){return b.y<a.y||(b.y==a.y&&b.x>a.x);}
uint3 rgb(uint c){return uint3(c&255,(c>>8)&255,(c>>16)&255);}
uint3 expand555(uint c){uint3 v=uint3(c&31,(c>>5)&31,(c>>10)&31);return (v<<3)|(v>>2);}
uint2 uv(uint c){return uint2(c&255,(c>>8)&255);}
int dither(int2 p){const int values[16]={-4,0,-3,1,2,-2,3,-1,-3,1,-4,0,3,-1,2,-2};
    return values[((p.y&3)*4)+(p.x&3)];}
uint texel(uint p,uint2 st){uint mx=(256-word(p,7))&248,my=(256-word(p,8))&248;
    uint u=((st.x&~mx)|(word(p,5)&mx))&255,v=((st.y&~my)|(word(p,6)&my))&255;
    uint page=word(p,2),depth=(page>>7)&3;int x=int((page&15)*64),y=int((page&16)*16);
    uint layer=word(p,40);
    if(depth==2)return vram(layer,x+int(u),y+int(v));
    uint index=depth==0?(vram(layer,x+int(u/4),y+int(v))>>((u&3)*4))&15:
        (vram(layer,x+int(u/2),y+int(v))>>((u&1)*8))&255;
    uint clut=word(p,3);return vram(layer,int((clut&63)*16+index),int((clut>>6)&511));}
bool hm_font_glyph(uint p){uint clut=word(p,3),row=clut>>6;
    return word(p,2)==14&&(clut&63)==56&&row>=250&&row<=254;}
uint interp_i(uint3 values,int3 w,int area){
    uint a=uint(area),num=uint(w.x)*values.x+uint(w.y)*values.y+uint(w.z)*values.z+a/2;
    uint reciprocal=0xffffffffu/a+((0xffffffffu%a)+1+a/2)/a;
    return hm_mul_u64(num,reciprocal).y;
}
uint interp_div(uint3 values,int3 w,int area){return (uint(w.x)*values.x+uint(w.y)*values.y+
    uint(w.z)*values.z+uint(area/2))/uint(area);}
uint interp_f(uint3 values,float3 w,float area){return uint(floor(dot(float3(values),w)/area+0.5));}
#include "../../enhance/filtering/texture.hlsl"
#include "../../enhance/antialiasing/msaa.hlsl"
#define HM_SCENE_BIN_TILE_SIZE 8
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID,uint3 group:SV_GroupID,uint3 lane:SV_GroupThreadID){
    uint width=load_word(0),height=load_word(1);
    int2 origin=int2(load_word(2),load_word(3));uint2 logical_size=uint2(load_word(4),load_word(5));
    uint tiles_x=load_word(6),bin_table=load_word(9);
    bool native=(width==logical_size.x&&height==logical_size.y);
    uint group_thread=lane.y*8+lane.x;
    if(group_thread==0){
        hm_group_tile_valid=native&&((width&7)==0)&&((height&7)==0);
        hm_group_cache_enabled=0;hm_group_bin=0;hm_group_count=0;
        if(hm_group_tile_valid){
            uint group_tile=group.y*tiles_x+group.x;
            hm_group_bin=load_word(bin_table+group_tile);
            hm_group_count=load_word(hm_group_bin);
            hm_group_cache_enabled=hm_group_count>0&&
                hm_group_count<=HM_SCENE_GROUP_PRIMITIVE_CAPACITY;
        }
    }
    GroupMemoryBarrierWithGroupSync();
    if(hm_group_cache_enabled){
        if(group_thread<hm_group_count)
            hm_group_primitive_ids[group_thread]=load_word(hm_group_bin+1+group_thread);
        GroupMemoryBarrierWithGroupSync();
        uint primitive_base=load_word(8);
        uint cache_word_count=hm_group_count*HM_SCENE_PRIMITIVE_STRIDE;
        [loop]for(uint cache_word=group_thread;cache_word<cache_word_count;cache_word+=64){
            uint cached_item=cache_word/HM_SCENE_PRIMITIVE_STRIDE;
            uint primitive_word=cache_word-cached_item*HM_SCENE_PRIMITIVE_STRIDE;
            uint source=primitive_base+hm_group_primitive_ids[cached_item]*HM_SCENE_PRIMITIVE_STRIDE+
                primitive_word;
            hm_group_primitive_words[cache_word]=load_word(source);
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if(id.x>=width||id.y>=height)return;
    /* Native scenes map one compute pixel to one logical pixel exactly. Avoid
     * two dynamic integer divisions per output pixel on mobile GPUs. */
    int2 local=native?int2(id.xy):
        int2(((2*id.xy+1)*logical_size)/(2*uint2(width,height)));
    int2 logical=origin+local;
    uint filter_mode=load_word(11);bool filtering_enabled=filter_mode!=0;
    uint color=(logical.x<int(load_word(12))||logical.x>=int(load_word(12)+load_word(13)))?
        load_word(14):vram(0,logical.x,logical.y);
    uint samples=load_word(15);if(samples!=2&&samples!=4)samples=1;
    uint sample_colors[4]={color,color,color,color};
    uint3 fine_colors[4]={expand555(color),expand555(color),expand555(color),expand555(color)};
    /* Header[9] points at tile offsets; each offset points to [count, ids...]. */
    uint tile=uint((local.y/HM_SCENE_BIN_TILE_SIZE)*int(tiles_x)+local.x/HM_SCENE_BIN_TILE_SIZE);
    uint bin=hm_group_tile_valid?hm_group_bin:load_word(bin_table+tile);
    uint count=hm_group_tile_valid?hm_group_count:load_word(bin);
    [loop]for(uint item=0;item<count;++item){
        uint primitive_index=hm_group_cache_enabled?hm_group_primitive_ids[item]:load_word(bin+1+item);
        uint p=hm_group_cache_enabled?0x80000000u+item*HM_SCENE_PRIMITIVE_STRIDE:
            load_word(8)+primitive_index*HM_SCENE_PRIMITIVE_STRIDE;
        uint op=word(p,1),flags=word(p,4);
        int2 clip=int2(word(p,9),word(p,10)),clip_end=clip+int2(word(p,11),word(p,12));
        if(any(logical<clip)||any(logical>=clip_end))continue;
        int2 display=int2(word(p,13),word(p,14)),display_end=display+int2(word(p,15),word(p,16));
        if(!(flags&2)&&!(flags&128)&&word(p,17)&&all(logical>=display)&&all(logical<display_end))continue;
        if(word(p,0)==2){int2 delta=logical-int2(word(p,30),word(p,31));
            if(any(delta<0)||delta.x>=int(word(p,32))||delta.y>=int(word(p,33)))continue;
            [unroll]for(uint transfer_mask_sample=0;transfer_mask_sample<4;++transfer_mask_sample){
                if(transfer_mask_sample>=samples)break;
                if((flags&8)&&(sample_colors[transfer_mask_sample]&32768))continue;
                uint transferred=word(p,1)==0xa0?vram(word(p,40),logical.x,logical.y):
                    word(p,1)==0x80?vram(word(p,40),int(word(p,34))+delta.x,int(word(p,35))+delta.y):
                    word(p,24);
                sample_colors[transfer_mask_sample]=transferred|((flags&4)?32768:0);
                fine_colors[transfer_mask_sample]=expand555(sample_colors[transfer_mask_sample]);
            }
            continue;}
        uint coverage=(1u<<samples)-1;
        [unroll]for(uint mask_sample=0;mask_sample<4;++mask_sample){if(mask_sample>=samples)break;
            if((flags&8)&&(sample_colors[mask_sample]&32768))coverage&=~(1u<<mask_sample);}
        if(!coverage)continue;
        uint3 light=rgb(word(p,24));uint2 st=uv(word(p,27));
        int2 filtered_uv=int2(st)*256;uint4 texture_bounds=uint4(st,st);
        bool textured=(op&4)!=0,raw=textured&&((op&1)!=0);
        bool filter_texture=filtering_enabled&&!(textured&&hm_font_glyph(p));
        bool exact_native=native&&!(flags&32)&&samples==1;
        if(word(p,0)==0){
            int2 a=int2(word(p,18),word(p,19)),b=int2(word(p,20),word(p,21)),c=int2(word(p,22),word(p,23));
            int3 wi=0;float3 wf=0;int ai=0;float af=0;
            if(exact_native){int2 center=2*logical+1;
                int2 aa=2*a,bb=2*b,cc=2*c;
                wi=int3(edge_i(bb,cc,center),edge_i(cc,aa,center),edge_i(aa,bb,center));ai=edge_i(aa,bb,cc);
                if(ai<=0||any(wi<0)||(wi.x==0&&!top_left(b,c))||(wi.y==0&&!top_left(c,a))||(wi.z==0&&!top_left(a,b)))continue;
            }else{float2 aa=float2(int2(word(p,34),word(p,35)))/256.0;
                float2 bb=float2(int2(word(p,36),word(p,37)))/256.0;
                float2 cc=float2(int2(word(p,38),word(p,39)))/256.0;
                float2 center=float2(id.xy)+0.5;
                int2 qa=int2(word(p,34),word(p,35)),qb=int2(word(p,36),word(p,37)),qc=int2(word(p,38),word(p,39));
                if(samples>1){coverage&=hm_aa_triangle(qa,qb,qc,id.xy,samples);if(!coverage)continue;
                    center=hm_aa_center(id.xy,coverage,samples);}
                wf=float3(edge_f(bb,cc,center),edge_f(cc,aa,center),edge_f(aa,bb,center));af=edge_f(aa,bb,cc);
                int2 sample_q8=int2(id.xy)*256+128;
                int3 signs=int3(hm_edge_sign(qb,qc,sample_q8,wf.x),hm_edge_sign(qc,qa,sample_q8,wf.y),hm_edge_sign(qa,qb,sample_q8,wf.z));
                if(af<=0)continue;
                if(samples==1&&(any(signs<0)||(signs.x==0&&!top_left_f(bb,cc))||(signs.y==0&&!top_left_f(cc,aa))||(signs.z==0&&!top_left_f(aa,bb))))continue;
                wf=max(wf,0);
            }
            uint3 r0=rgb(word(p,24)),r1=rgb(word(p,25)),r2=rgb(word(p,26));
            uint2 t0=uv(word(p,27)),t1=uv(word(p,28)),t2=uv(word(p,29));
            texture_bounds=uint4(min(t0,min(t1,t2)),max(t0,max(t1,t2)));
            if(exact_native){if((op&16)&&!raw&&!(all(r0==r1)&&all(r1==r2)))
                light=filtering_enabled?uint3(interp_div(uint3(r0.x,r1.x,r2.x),wi,ai),interp_div(uint3(r0.y,r1.y,r2.y),wi,ai),interp_div(uint3(r0.z,r1.z,r2.z),wi,ai)):
                    uint3(interp_i(uint3(r0.x,r1.x,r2.x),wi,ai),interp_i(uint3(r0.y,r1.y,r2.y),wi,ai),interp_i(uint3(r0.z,r1.z,r2.z),wi,ai));
                if(textured){if(filter_texture)filtered_uv=int2(hm_filter_uv_i(uint3(t0.x,t1.x,t2.x),wi,ai),hm_filter_uv_i(uint3(t0.y,t1.y,t2.y),wi,ai));
                    else st=uint2(interp_i(uint3(t0.x,t1.x,t2.x),wi,ai),interp_i(uint3(t0.y,t1.y,t2.y),wi,ai));}}
            else{if((op&16)&&!raw)light=uint3(interp_f(uint3(r0.x,r1.x,r2.x),wf,af),interp_f(uint3(r0.y,r1.y,r2.y),wf,af),interp_f(uint3(r0.z,r1.z,r2.z),wf,af));
                if(textured){float3 wp=wf;float ap=af;
                    if(word(p,33)!=0){float3 zv=float3(asfloat(word(p,30)),asfloat(word(p,31)),asfloat(word(p,32)));
                        wp=wf/max(zv,1.0);float sw=dot(wp,float3(1,1,1));if(sw>0.0)ap=sw;}
                    if(filter_texture)filtered_uv=int2(hm_filter_uv_f(uint3(t0.x,t1.x,t2.x),wp,ap),hm_filter_uv_f(uint3(t0.y,t1.y,t2.y),wp,ap));
                    else st=uint2(interp_f(uint3(t0.x,t1.x,t2.x),wp,ap),interp_f(uint3(t0.y,t1.y,t2.y),wp,ap));}}
        }else{int2 delta;
            texture_bounds=uint4(st,st+uint2(word(p,32),word(p,33))-1);
            if(flags&32){float2 low=float2(int2(word(p,35),word(p,36)))/256.0;
                float2 high=float2(int2(word(p,37),word(p,38)))/256.0,center=float2(id.xy)+0.5;
                if(samples>1){coverage&=hm_aa_rect(int2(word(p,35),word(p,36)),int2(word(p,37),word(p,38)),id.xy,samples);
                    if(!coverage)continue;center=hm_aa_center(id.xy,coverage,samples);}
                else if(any(center<low)||any(center>=high))continue;
                delta=int2(floor((center-low)*float2(logical_size)/float2(width,height)));
                if(filter_texture){int2 fixed_center=int2(floor(center*256.0))-int2(word(p,35),word(p,36));
                    filtered_uv=int2(st)*256+int2((uint2(fixed_center)*logical_size+uint2(width,height)/2)/uint2(width,height))-128;}
            }else{delta=logical-int2(word(p,30),word(p,31));
                if(any(delta<0)||delta.x>=int(word(p,32))||delta.y>=int(word(p,33)))continue;
                if(filter_texture){uint2 center_q8=((2*id.xy+1)*logical_size*128+uint2(width,height)/2)/uint2(width,height);
                    filtered_uv=int2(st)*256+int2(center_q8)+(origin-int2(word(p,30),word(p,31)))*256-128;}}
            st+=uint2(delta);}
        if(textured&&filter_texture){
            [unroll]for(uint filtered_sample=0;filtered_sample<4;++filtered_sample){if(filtered_sample>=samples)break;
                if(coverage&(1u<<filtered_sample)){
                    sample_colors[filtered_sample]=hm_filter_compose(p,filtered_uv,texture_bounds,light,logical,sample_colors[filtered_sample],filter_mode);
                    fine_colors[filtered_sample]=expand555(sample_colors[filtered_sample]);}}
            continue;}
        uint sample_color=textured?texel(p,st):0;if(textured&&sample_color==0)continue;
        uint source=sample_color&32768;
        uint3 smooth_color=light;
        if(raw)source=sample_color;
        else{int3 value=int3(light);int3 tex=int3(sample_color&31,(sample_color>>5)&31,(sample_color>>10)&31);
            if(word(p,0)==0){if(textured)value=value*tex/16;
                if((flags&1)&&((op&16)||(textured&&!raw)))value+=dither(logical);
                smooth_color=uint3(clamp(value,0,255));
                value=clamp(value,0,255)>>3;
            }else{if(textured)value=min(tex*value/128,31);
                else{if(word(p,34)&&(flags&1))value=clamp(value+dither(logical),0,255);value>>=3;}}
            source|=uint(value.x|(value.y<<5)|(value.z<<10));}
        [unroll]for(uint blend_sample=0;blend_sample<4;++blend_sample){if(blend_sample>=samples)break;
        if(!(coverage&(1u<<blend_sample)))continue;
        uint result=source,background=sample_colors[blend_sample];
        if((op&2)&&(!textured||(sample_color&32768))){
            int3 back=int3(background&31,(background>>5)&31,(background>>10)&31),front=int3(source&31,(source>>5)&31,(source>>10)&31);
            uint mode=(word(p,2)>>5)&3;int3 mixed=mode==0?back/2+front/2:mode==1?back+front:mode==2?back-front:back+front/4;
            mixed=clamp(mixed,0,31);result=(source&32768)|uint(mixed.x|(mixed.y<<5)|(mixed.z<<10));}
        sample_colors[blend_sample]=result|((flags&4)?32768:0);
        fine_colors[blend_sample]=(flags&64)&&word(p,0)==0&&!raw&&!(op&2)&&((op&16)||textured)
            ? smooth_color : expand555(result);}
    }
    uint3 sum=0;
    [unroll]for(uint resolve_sample=0;resolve_sample<4;++resolve_sample){if(resolve_sample>=samples)break;
        sum+=fine_colors[resolve_sample];}
    uint3 c=(sum+samples/2)/samples;
    output_image[id.xy]=float4(float3(c)/255.0,1.0);
}
