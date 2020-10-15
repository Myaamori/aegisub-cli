// Copyright (c) 2010, Amar Takhar <verm@aegisub.org>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include "command.h"

#include <libaegisub/format.h>
#include <libaegisub/log.h>

namespace cmd {
	static std::map<std::string, std::unique_ptr<Command>> cmd_map;
	typedef std::map<std::string, std::unique_ptr<Command>>::iterator iterator;

	static iterator find_command(std::string const& name) {
		auto it = cmd_map.find(name);
		if (it == cmd_map.end())
			throw CommandNotFound(agi::format("'%s' is not a valid command name", name));
		return it;
	}

	void reg(std::unique_ptr<Command> cmd) {
		cmd_map[cmd->name()] = std::move(cmd);
	}

	void unreg(std::string const& name) {
		cmd_map.erase(find_command(name));
	}

	Command *get(std::string const& name) {
		return find_command(name)->second.get();
	}

	void call(std::string const& name, agi::Context*c) {
		Command &cmd = *find_command(name)->second;
		if (cmd.Validate(c))
			cmd(c);
	}

	std::vector<std::string> get_registered_commands() {
		std::vector<std::string> ret;
		ret.reserve(cmd_map.size());
		for (auto const& it : cmd_map)
			ret.push_back(it.first);
		return ret;
	}

	void init_builtin_commands() {
		LOG_D("command/init") << "Populating command map";
	}

	void clear() {
		cmd_map.clear();
	}
}
