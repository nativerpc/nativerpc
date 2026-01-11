##
#   Native RPC Python Language Extensions
#
#       parseInt
#       getHeaderMap
##

def parseInt(value):
    try:
        return int(value)
    except ValueError:
        pass
    return 0


def getHeaderMap(headers, names):
    result = {}
    headers = headers.lower()
    for name in names:
        colonName = name.lower() + ":"
        if colonName not in headers:
            result[name] = ""
            continue
        startIndex = headers.index(colonName) + len(colonName)
        endIndex = len(headers)
        try:
            endIndex = headers.index("\n", startIndex)
        except Exception:
            pass
        result[name] = headers[startIndex:endIndex].strip()

    return result
