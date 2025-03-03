import sys

if len(sys.argv) < 5:
    print(f'usage: {sys.argv[0]} <platform name> <class name> <base subdir> <constructor parameters>')
    exit(1)

platform_name = sys.argv[1]
class_name = sys.argv[2]
base_subdir = sys.argv[3]
constructor_parameters = sys.argv[4]

if base_subdir != '':
	base_subdir = '/' + base_subdir

parameter_names = [];
for parameter in constructor_parameters.split(' '):
	if (parameter.endswith(',')):
		parameter_names.append(parameter[:-1])

if (len(constructor_parameters) > 0):
	parameter_names.append(constructor_parameters.split(' ')[-1])

base_class_header = f'''#pragma once

#include <memory>

namespace Hydrogen
{{
	class {class_name}
	{{
	public:
		virtual ~{class_name}() = default;

		static std::shared_ptr<{class_name}> Create({constructor_parameters});
	}};
}}
'''

base_class_impl = f'''#include "Hydrogen{base_subdir}/{class_name}.hpp"
#include "Hydrogen/Platform/{platform_name}/{platform_name}{class_name}.hpp"

using namespace Hydrogen;

std::shared_ptr<{class_name}> {class_name}::Create({constructor_parameters})
{{
	return std::make_shared<{platform_name}{class_name}>({', '.join(parameter_names)});
}}
'''

platform_class_header = f'''#include "Hydrogen{base_subdir}/{class_name}.hpp"

namespace Hydrogen
{{
	class {platform_name}{class_name} : public {class_name}
	{{
	public:
		{platform_name}{class_name}({constructor_parameters});
		~{platform_name}{class_name}();
	}};
}}
'''

platform_class_impl = f'''#include "Hydrogen/Platform/{platform_name}/{platform_name}{class_name}.hpp"

using namespace Hydrogen;

{platform_name}{class_name}::{platform_name}{class_name}({constructor_parameters})
{{
}}

{platform_name}{class_name}::~{platform_name}{class_name}()
{{
}}
'''

platform_class_header_file = f'../HydrogenEngine/Include/Hydrogen/Platform/{platform_name}/{platform_name}{class_name}.hpp'
platform_class_impl_file = f'../HydrogenEngine/Source/Platform/{platform_name}/{platform_name}{class_name}.cpp'

base_class_header_file = f'../HydrogenEngine/Include/Hydrogen{base_subdir}/{class_name}.hpp'
base_class_impl_file = f'../HydrogenEngine/Source{base_subdir}/{class_name}.cpp'

with open(platform_class_header_file, 'w') as f:
	f.write(platform_class_header)

with open(platform_class_impl_file, 'w') as f:
	f.write(platform_class_impl)

with open(base_class_header_file, 'w') as f:
	f.write(base_class_header)

with open(base_class_impl_file, 'w') as f:
	f.write(base_class_impl)
