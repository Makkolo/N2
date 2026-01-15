#include "tools.hpp"










int extractString(std::string *ref, std::string *out, const char seperator)
{
    size_t end = ref->find(seperator);
    if (end == std::string::npos)
    {
        if (ref->length()<=0)
            return -1;
        *out = *ref;
        *ref = "";
    }
    else
    {
        *out = ref->substr(0,end);
        ref->erase(0,end+1);
    }
    return 0;
}










void cleanString(std::string *str, const char remove[], int rmSize)
{
	size_t pos, count;
	for (int i = 0; i<rmSize; i++)
	{
		pos = str->find(remove[i]);
		while (pos != std::string::npos)
		{
			count = 1;
			while (str->find(remove[i], pos + count))
				count++;
			str->erase(pos, count);
			str->find(remove[i]);
		}
	}
}