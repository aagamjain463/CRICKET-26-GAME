import unreal as u,runpy
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve()
runpy.run_path(str(root/'Tools/ApplyPremiumBodyMaterials.py'),run_name='__main__')
runpy.run_path(str(root/'Tools/DiagnosePremiumRun.py'),run_name='__main__')
