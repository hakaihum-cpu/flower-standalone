package com.analoglav.amps;

/** Configurable AAudio trial values. These do not control USB isochronous URBs. */
public final class TuningProfile {
  public static final int[][] OPTIONS={
    {0,96,128,192,256,384,480,720,960,1440,1920},
    {0,96,128,192,256,480,960},
    {0,192,384,480,960,1920},
    {0,1}
  };
  public static final String[] NAMES={"OUT BUFFER FRAMES","CALLBACK FRAMES","INPUT BUFFER FRAMES","SHARING MODE"};
  private final int[] selected={0,0,0,0};
  public int get(int row){return OPTIONS[row][selected[row]];}
  public int index(int row){return selected[row];}
  public void setIndex(int row,int idx){
    if(row<0 || row>=OPTIONS.length)throw new IllegalArgumentException("row");
    if(idx<0||idx>=OPTIONS[row].length)throw new IllegalArgumentException("index");
    selected[row]=idx;
  }
  public void setValue(int row,int value){
    for(int j=0;j<OPTIONS[row].length;j++)if(OPTIONS[row][j]==value){selected[row]=j;return;}
    selected[row]=0;
  }
  public void next(int row){setIndex(row,(selected[row]+1)%OPTIONS[row].length);}
  public void previous(int row){setIndex(row,(selected[row]+OPTIONS[row].length-1)%OPTIONS[row].length);}
  public void reset(){for(int i=0;i<selected.length;i++)selected[i]=0;}
  public String label(int row){
    int value=get(row);
    if(row==3)return value==0?"PREFER EXCLUSIVE":"SHARED ONLY";
    return value==0?"AUTO":value+" FR";
  }
  public String report(){
    StringBuilder sb=new StringBuilder();
    for(int row=0;row<OPTIONS.length;row++)sb.append(NAMES[row]).append('=').append(label(row)).append('\n');
    return sb.toString();
  }
}
