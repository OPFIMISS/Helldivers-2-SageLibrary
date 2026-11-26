using HellForge.Core;
using HellForge.Core.GameData;

static class BuildCloak
{
    public static void Run(GameDataReader game, string root)
    {
        // base_water_refraction has no BLENDINDICES/BLENDWEIGHTS input and
        // renders character surfaces in bind pose. The resident glass shader
        // provides both rigid and skinned refraction variants.
        const ulong refractionBase = 0xAC39020959C7E54B;
        var resources = new Dictionary<ResourceKey, ResourceParts>();
        var refract = Read(refractionBase);
        var shader = System.Text.Encoding.ASCII.GetString(refract.Gpu);
        if (!shader.Contains("BLENDINDICES") || !shader.Contains("BLENDWEIGHTS") ||
            !shader.Contains("c_skin_matrices") || !shader.Contains("__tex_hdr0"))
            throw new InvalidDataException("Refraction base must support skeletal animation and scene color");
        var bytes = refract.Main.ToArray();
        // Base materials embed an archive-relative shader offset. Make an
        // instance referencing the resident base instead of copying that offset.
        BitConverter.GetBytes(0ul).CopyTo(bytes, 0x10);
        BitConverter.GetBytes(refractionBase).CopyTo(bytes, 0x18);
        Scalar(bytes, "ior_intensity", 0.018f);
        Scalar(bytes, "noise_distortion_amount", 0.012f);
        Scalar(bytes, "ior_noise_tiling", 1.75f);
        Scalar(bytes, "flow_speed_max", 0.12f);
        Scalar(bytes, "glass_fog", 0f);
        Scalar(bytes, "liquid_fog", 0f);
        Scalar(bytes, "water_disruption_fog_alpha", 0f);
        Scalar(bytes, "glass_thickness", 0.03f);
        Scalar(bytes, "roughness_intensity", 0.08f);
        Scalar(bytes, "specular", 0f);
        Scalar(bytes, "emissive_intensity", 0f);
        Scalar(bytes, "bounds_mask", 0f);
        Scalar(bytes, "ior_mask_alpha_threshold", 0f);
        Parameter(bytes, "glass_color", 2, [1f, 1f, 1f]);
        Parameter(bytes, "liquid_color", 2, [1f, 1f, 1f]);
        Texture(bytes, "water_normal_map", ResourceTypes.Hash("content/fac_bugs/cha_stalker/cha_stalker_normal"));
        Texture(bytes, "water_noise", ResourceTypes.Hash("content/fac_bugs/cha_stalker/cha_stalker_cloak"));

        var stalker = Read(ResourceTypes.Hash("content/fac_bugs/cha_stalker/m_stalker"));
        var fallback = stalker.Main.ToArray();
        Scalar(fallback, "opacity_stealth", 0f);
        foreach (var (folder, prefix) in new[] {
            ("Optical-Module-0.4", "liuli/optical_module/"),
            ("Field-Support-0.4", "liuli/field_support/")
        }) {
            resources.Clear();
            var id = ResourceTypes.Hash(prefix + "materials/optical_refraction");
            resources[new(ResourceTypes.Material, id)] = new(bytes.ToArray(), refract.Gpu, refract.Stream);
            var fallbackId = ResourceTypes.Hash(prefix + "materials/optical_cloak");
            resources[new(ResourceTypes.Material, fallbackId)] = new(fallback.ToArray(), stalker.Gpu, stalker.Stream);
            var archive = ResourceArchive.Write(resources);
            var path = Path.Combine(Path.GetFullPath(root), folder, "data", "9ba626afa44a3aa3.patch_1");
            File.WriteAllBytes(path, archive.Archive);
            if (archive.Gpu.Length > 0) File.WriteAllBytes(path + ".gpu_resources", archive.Gpu);
            if (archive.Stream.Length > 0) File.WriteAllBytes(path + ".stream", archive.Stream);
            Console.WriteLine($"{folder}: refraction={id:x16}, fallback={fallbackId:x16}, {archive.Archive.Length} bytes");
        }

        ResourceParts Read(ulong id) {
            var key = new ResourceKey(ResourceTypes.Material, id);
            var found = game.Find([key]);
            return game.Read(found[key]);
        }
    }

    static void Scalar(byte[] bytes, string name, float value) => Parameter(bytes, name, 0, [value]);

    static void Parameter(byte[] bytes, string name, uint type, float[] values)
    {
        var textures = checked((int)BitConverter.ToUInt32(bytes, 0x40));
        var count = checked((int)BitConverter.ToUInt32(bytes, 0x68));
        var start = 0x88 + textures * 12;
        var data = start + count * 20;
        for (var i = 0; i < count; ++i) {
            var at = start + i * 20;
            if (BitConverter.ToUInt32(bytes, at + 8) != EngineHash.Id32(name)) continue;
            if (BitConverter.ToUInt32(bytes, at) != type) throw new InvalidDataException(name + " type mismatch");
            var offset = data + checked((int)BitConverter.ToUInt32(bytes, at + 12));
            for (var j = 0; j < values.Length; ++j) BitConverter.GetBytes(values[j]).CopyTo(bytes, offset + 4*j);
            return;
        }
        throw new InvalidDataException("Missing parameter " + name);
    }

    static void Texture(byte[] bytes, string name, ulong resource)
    {
        var count = checked((int)BitConverter.ToUInt32(bytes, 0x40));
        for (var i = 0; i < count; ++i) {
            if (BitConverter.ToUInt32(bytes, 0x88 + i * 4) != EngineHash.Id32(name)) continue;
            BitConverter.GetBytes(resource).CopyTo(bytes, 0x88 + count * 4 + i * 8);
            return;
        }
        throw new InvalidDataException("Missing texture " + name);
    }
}
