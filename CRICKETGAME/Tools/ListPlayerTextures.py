import unreal
assets = unreal.EditorAssetLibrary.list_assets('/Game/Cricket26/Textures/Players')
for a in assets:
    print("TEX:", a)
mats = unreal.EditorAssetLibrary.list_assets('/Game/Cricket26/Materials/Players')
for m in mats:
    print("MAT:", m)
