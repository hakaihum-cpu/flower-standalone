package com.analoglav.hydrasynthcontroller;

import android.content.Context;
import android.util.AtomicFile;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Base64;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Android private editor documents. For acquired patches retains the complete
 * 2790-byte original as a baseline, plus editable mapped values as an overlay.
 * The raw baseline is NOT updated when values are edited: export is not yet supported.
 */
public final class PatchLibrary {
    public static final class Document {
        public final String name;
        public final LinkedHashMap<String,Integer> values;
        /** Immutable copy of a complete acquired Hydrasynth slot, if known. */
        public final byte[] sourcePatch;
        Document(String name, LinkedHashMap<String,Integer> values, byte[] sourcePatch) {
            this.name=name; this.values=values;
            this.sourcePatch=sourcePatch==null?null:Arrays.copyOf(sourcePatch,sourcePatch.length);
        }
    }
    private final AtomicFile storage;
    public PatchLibrary(Context context) {
        storage = new AtomicFile(new File(context.getFilesDir(),"hydra_dot_presets_v1.json"));
    }
    private JSONObject read() throws Exception {
        if (!storage.getBaseFile().exists()) {
            JSONObject root = new JSONObject();
            root.put("schema",1);
            root.put("documents",new JSONArray());
            return root;
        }
        try (FileInputStream stream=storage.openRead();
             ByteArrayOutputStream buffer=new ByteArrayOutputStream()) {
            byte[] bytes=new byte[4096];
            int count;
            while((count=stream.read(bytes))!=-1)buffer.write(bytes,0,count);
            JSONObject parsed=new JSONObject(new String(buffer.toByteArray(), StandardCharsets.UTF_8));
            if(parsed.optInt("schema",-1)!=1)throw new IllegalStateException("Unsupported preset schema");
            if(parsed.optJSONArray("documents")==null)throw new IllegalStateException("Malformed library");
            return parsed;
        }
    }
    private void write(JSONObject root) throws Exception {
        FileOutputStream out=null;
        try {
            out=storage.startWrite();
            out.write(root.toString().getBytes(StandardCharsets.UTF_8));
            storage.finishWrite(out);
        } catch(Exception error) {
            if(out!=null) storage.failWrite(out);
            throw error;
        }
    }
    public List<String> names() throws Exception {
        JSONArray docs=read().getJSONArray("documents");
        ArrayList<String> result=new ArrayList<>();
        for(int i=0;i<docs.length();i++) result.add(docs.getJSONObject(i).getString("name"));
        return result;
    }
    public void save(String name,Map<String,Integer> values) throws Exception {
        save(name,values,null);
    }
    public void save(String name,Map<String,Integer> values,byte[] rawBaseline) throws Exception {
        name=validateName(name);
        if(rawBaseline!=null&&rawBaseline.length!=2790)
            throw new IllegalArgumentException("Only complete 2790-byte patch baselines may be saved");
        JSONObject root=read();
        JSONArray existing=root.getJSONArray("documents");
        JSONArray next=new JSONArray();
        for(int i=0;i<existing.length();i++) {
            JSONObject d=existing.getJSONObject(i);
            if(!name.equals(d.getString("name")))next.put(d);
        }
        JSONObject document=new JSONObject();
        document.put("name",name);
        JSONObject assignments=new JSONObject();
        for(Map.Entry<String,Integer> e:values.entrySet()) {
            if(e.getKey()!=null && e.getValue()!=null)assignments.put(e.getKey(),e.getValue());
        }
        document.put("values",assignments);
        if(rawBaseline!=null)
            document.put("rawBaseline",Base64.getEncoder().encodeToString(rawBaseline));
        next.put(document);
        root.put("documents",next);
        write(root);
    }
    public Document load(String name) throws Exception {
        JSONArray docs=read().getJSONArray("documents");
        for(int i=0;i<docs.length();i++){
            JSONObject d=docs.getJSONObject(i);
            if(!name.equals(d.getString("name")))continue;
            JSONObject a=d.getJSONObject("values");
            LinkedHashMap<String,Integer> map=new LinkedHashMap<>();
            java.util.Iterator<String> iter=a.keys();
            while(iter.hasNext()){
                String key=iter.next();
                int v=a.getInt(key);
                ParameterCatalog.Param p=ParameterCatalog.find(key);
                if(p!=null && v>=p.min && v<=p.max)map.put(key,v);
            }
            byte[] raw=null;
            if(d.has("rawBaseline")){
                raw=Base64.getDecoder().decode(d.getString("rawBaseline"));
                if(raw.length!=2790)throw new IllegalArgumentException("Corrupted raw baseline");
                new HydraPatchSnapshot(raw);  // validate slot metadata
            }
            return new Document(name,map,raw);
        }
        throw new IllegalArgumentException("Preset not found");
    }
    public void delete(String name) throws Exception {
        JSONObject root=read();
        JSONArray existing=root.getJSONArray("documents");
        JSONArray next=new JSONArray();
        boolean found=false;
        for(int i=0;i<existing.length();i++){
            JSONObject d=existing.getJSONObject(i);
            if(name.equals(d.getString("name")))found=true;
            else next.put(d);
        }
        if(!found)throw new IllegalArgumentException("Preset not found");
        root.put("documents",next);write(root);
    }
    private static String validateName(String name) {
        if(name==null)throw new IllegalArgumentException("Name required");
        String n=name.trim();
        if(n.length()<1||n.length()>32)throw new IllegalArgumentException("Name must be 1-32 characters");
        return n;
    }
}
