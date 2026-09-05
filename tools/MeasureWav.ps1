param([Parameter(Mandatory=$true)][string[]]$Paths)
$ErrorActionPreference='Stop'
# Only reads our canonical stereo PCM16 / 48 kHz regression renders.
Add-Type -TypeDefinition @'
using System;
using System.IO;
public static class DopaRenderMetrics {
  public static double[] Read(string path) {
    var b=File.ReadAllBytes(path);
    if(b.Length<44 || BitConverter.ToUInt16(b,20)!=1 || BitConverter.ToUInt16(b,22)!=2 || BitConverter.ToInt32(b,24)!=48000 || BitConverter.ToUInt16(b,34)!=16 || System.Text.Encoding.ASCII.GetString(b,36,4)!="data") throw new Exception("Expected canonical regression WAV");
    double energy=0,peak=0; int n=(b.Length-44)/2;
    for(int i=44;i+1<b.Length;i+=2){double x=BitConverter.ToInt16(b,i)/32768.0;energy+=x*x;peak=Math.Max(peak,Math.Abs(x));}
    return new[]{20*Math.Log10(Math.Max(1e-12,peak)),10*Math.Log10(Math.Max(1e-24,energy/n))};
  }
}
'@
foreach($wavePath in $Paths){$result=[DopaRenderMetrics]::Read((Resolve-Path -LiteralPath $wavePath).Path);[pscustomobject]@{File=$wavePath;Peak_dBFS=[math]::Round($result[0],2);RMS_dBFS=[math]::Round($result[1],2)}}
