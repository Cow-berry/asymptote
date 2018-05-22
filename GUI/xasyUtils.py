def tryParse(val, typ=float):
    try:
        return typ(val)
    except ValueError:
        return None


def listize(str, typ, delim='()'):
    str = str.strip(delim)
    raw_elem = str.split(',')
    final_list = []
    if isinstance(typ, (list, tuple)):
        for i in range(len(raw_elem)):
            if i < len(typ):
                curr_typ = typ[i]
            else:
                curr_typ = typ[-1]
            final_list.append(curr_typ(raw_elem[i].strip()))
    else:
        for elem in raw_elem:
            final_list.append(typ(elem.strip()))
    return final_list