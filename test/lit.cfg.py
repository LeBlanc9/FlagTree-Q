import os
import shlex

import lit.formats

config.name = "FlagTree-Q"
config.test_format = lit.formats.ShTest(execute_external=False)
config.suffixes = [".mlir"]
config.test_source_root = os.path.join(config.flagtree_q_source_dir, "test")
config.test_exec_root = os.path.join(config.flagtree_q_binary_dir, "test")
config.excludes = ["Inputs", "python"]
config.substitutions.append(
    ("%flagtree_q_opt", shlex.quote(os.path.join(config.flagtree_q_tools_dir, "flagtree-q-opt")))
)
config.substitutions.append(("%FileCheck", shlex.quote(config.filecheck)))
