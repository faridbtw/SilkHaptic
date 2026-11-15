using System;
using System.IO;
using System.Collections.Generic;
using System.Text;
using AssetsTools.NET;
using AssetsTools.NET.Extra;
using Fmod5Sharp;

namespace HapticsBridge
{
    internal static partial class ClipExtractor
    {
        private sealed class HollowKnightClip
        {
            public string Name, Resource;
            public long Offset, Size;
        }

        private static int ExtractHollowKnight(string dataDir, string outDir, Action<int, int> progress)
        {
            var manager = new AssetsManager();
            var clips = new List<HollowKnightClip>();
            try
            {
                var assets = manager.LoadAssetsFile(Path.Combine(dataDir, "resources.assets"), false);
                foreach (var info in assets.file.GetAssetsOfType(AssetClassID.AudioClip))
                {
                    // The updated macOS build strips type trees. AudioClip has a
                    // fixed Unity 5+ serialized layout; no class database is needed.
                    // Field layout reference: github.com/Perfare/AssetStudio/blob/master/AssetStudio/Classes/AudioClip.cs
                    if (info.ByteSize > 1024 * 1024) throw new InvalidDataException("Unexpected AudioClip metadata size.");
                    var reader = assets.file.Reader;
                    reader.Position = info.GetAbsoluteByteOffset(assets.file);
                    using (var binary = new BinaryReader(new MemoryStream(reader.ReadBytes((int)info.ByteSize))))
                    {
                        string name = ReadUnityString(binary);
                        if (!name.EndsWith("_vibration", StringComparison.OrdinalIgnoreCase)) continue;
                        if (name != Path.GetFileName(name)) throw new InvalidDataException("Invalid haptic clip name.");
                        binary.ReadInt32(); // load type
                        int channels = binary.ReadInt32(), rate = binary.ReadInt32();
                        binary.ReadInt32(); binary.ReadSingle(); // bit depth, duration
                        binary.ReadBoolean(); AlignUnity(binary);
                        binary.ReadInt32(); // subsound index
                        binary.ReadBytes(3); AlignUnity(binary); // preload/background/legacy flags
                        string resource = ReadUnityString(binary);
                        long offset = binary.ReadInt64(), size = binary.ReadInt64();
                        if ((channels != 1 && channels != 2) || rate != 48000 || offset < 0 || size <= 0 || size > int.MaxValue)
                            throw new InvalidDataException("Unsupported haptic metadata for " + name);
                        clips.Add(new HollowKnightClip { Name = name, Resource = resource, Offset = offset, Size = size });
                    }
                }
            }
            finally { manager.UnloadAll(); }
            if (clips.Count == 0)
                throw new InvalidDataException("No PS5 haptic clips found. Hollow Knight's updated Unity 6 build is required.");

            Directory.CreateDirectory(outDir);
            int done = 0;
            foreach (var clip in clips)
            {
                string resourcePath = Path.Combine(dataDir, Path.GetFileName(clip.Resource.Replace('\\', '/')));
                using (var stream = File.OpenRead(resourcePath))
                {
                    if (clip.Offset > stream.Length || clip.Size > stream.Length - clip.Offset)
                        throw new InvalidDataException("Haptic resource range is invalid: " + clip.Name);
                    stream.Position = clip.Offset;
                    byte[] bytes = new byte[(int)clip.Size];
                    int read = 0;
                    while (read < bytes.Length)
                    {
                        int n = stream.Read(bytes, read, bytes.Length - read);
                        if (n == 0) throw new EndOfStreamException("Incomplete haptic data: " + clip.Name);
                        read += n;
                    }
                    var bank = FsbLoader.LoadFsbFromByteArray(bytes);
                    if (bank.Samples.Count != 1) throw new InvalidDataException("Unexpected haptic sample count: " + clip.Name);
                    var sample = bank.Samples[0];
                    byte[] decoded; string extension;
                    if (!sample.RebuildAsStandardFileFormat(out decoded, out extension))
                        throw new InvalidDataException("Cannot decode haptic waveform: " + clip.Name);
                    string path = Path.Combine(outDir, clip.Name + ".wav");
                    if (extension == "ogg") DecodeOggToWav(decoded, (int)sample.Metadata.SampleCount, path);
                    else if (extension == "wav") File.WriteAllBytes(path, decoded);
                    else throw new InvalidDataException("Unsupported haptic format: " + extension);
                }
                ++done;
                if (progress != null) progress(done, clips.Count);
            }
            Console.WriteLine("Hollow Knight: prepared " + done + " PS5 haptic clips.");
            return done;
        }

        private static void AlignUnity(BinaryReader reader)
        {
            long next = (reader.BaseStream.Position + 3) & ~3L;
            if (next > reader.BaseStream.Length) throw new EndOfStreamException("Invalid AudioClip alignment.");
            reader.BaseStream.Position = next;
        }

        private static string ReadUnityString(BinaryReader reader)
        {
            int count = reader.ReadInt32();
            if (count < 0 || count > 4096 || count > reader.BaseStream.Length - reader.BaseStream.Position)
                throw new InvalidDataException("Invalid AudioClip string length.");
            string value = Encoding.UTF8.GetString(reader.ReadBytes(count));
            AlignUnity(reader);
            return value;
        }
    }
}
