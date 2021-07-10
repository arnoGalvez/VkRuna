// Copyright (c) 2021 Arno Galvez

template<typename CharT>
std::vector<CharT> ReadBinary(const char *relativePath)
{
  std::basic_ifstream<CharT> file(relativePath, std::ios::ate | std::ios::binary);
  if (file.fail())
  {
    throw std::ios::failure("Failed to load file \"" + std::string(relativePath) + "\".");
  }

  auto end = file.tellg();
  file.seekg(0, std::ios::beg);
  auto begin = file.tellg();

  std::vector<CharT> buff(static_cast<size_t>(end - begin));
  file.read(buff.data(), end - begin);

  return buff;
}
