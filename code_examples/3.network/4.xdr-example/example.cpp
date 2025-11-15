// Структура для передачи
struct my_data
{
    bool bool_value;
    char char_value;
    int integer_value;
    double double_value;
    char buffer[10];
}
static char __buf[255];

// Упаковка и распаковка структуры
size_t serialize(my_data* data, void* buf, size_t buf_size, bool pack)
{
    XDR* _xdr = (XDR*)malloc(sizeof(XDR));
    size_t sz;
    xdrmem_create(_xdr, (caddr_t)buf, buf_size, pack?XDR_ENCODE:XDR_DECODE);
    xdr_uint8_t(_xdr, &data->bool_value);
    xdr_char(_xdr, &data->char_value);
    xdr_int(_xdr, &data->integer_value);
    xdr_double(_xdr, &data->double_value);
    xdr_array(_xdr, &data->buffer, &sz, 10, sizeof(char), xdr_char);
    sz = xdr_getpos(_xdr);
    xdr_destroy(_xdr);
    free(_xdr);
    return sz;
}
// Отправить данные
bool send_my_data(my_data* data)
{
	size_t pk = serialize(data,__buf,sizeof(__buf),true);
    return (send(m_socket, __buf, pk,0) > 0);
}
// Получить данные
bool recv_data(my_data* data)
{
    int res = recv(m_socket,__buf,sizeof(__buf),0);
    if (res <= 0)
        return false;
	size_t upk = serialize(data,__buf,res,false);
	return true;
}

