use std::io::{Read, Write};
use std::net::TcpStream;

#[derive(Debug, Copy, Clone)]
pub struct Pixel {
    pub x: usize,
    pub y: usize,
    pub color: (u8, u8, u8),
}

#[derive(Debug, Copy, Clone)]
pub struct Rect {
    pub x: usize,
    pub y: usize,
    pub w: usize,
    pub h: usize,
}

#[derive(Debug)]
struct ServerInfo {
    width: u32,
    height: u32,
    recv_buffer_size: u32,
    send_buffer_size: u32,
}

fn decode_u32(data: &[u8]) -> u32 {
    (data[0] as u32)
        | ((data[1] as u32) << 8)
        | ((data[2] as u32) << 16)
        | ((data[3] as u32) << 24)
}

fn command_info(stream: &mut TcpStream) -> std::io::Result<ServerInfo> {
    let mut data = [0u8; 8];
    data[0] = b'I';
    stream.write_all(&data[..])?;
    let mut response = [0u8; 16];
    stream.read_exact(&mut response[..])?;
    let width = decode_u32(&response[0..4]);
    let height = decode_u32(&response[4..8]);
    let recv_buffer_size = decode_u32(&response[8..12]);
    let send_buffer_size = decode_u32(&response[12..16]);
    Ok(ServerInfo { width, height, recv_buffer_size, send_buffer_size })
}

fn command_print(px: &Pixel, stream: &mut TcpStream) -> std::io::Result<()> {
    let mut data = [0u8; 8];
    data[0] = b'P';
    data[1] = px.x as u8;
    data[2] = (px.x >> 8) as u8;
    data[3] = px.y as u8;
    data[4] = (px.y >> 8) as u8;
    data[5] = px.color.0;
    data[6] = px.color.1;
    data[7] = px.color.2;
    stream.write_all(&data[..])?;
    Ok(())
}

fn command_get(px: &mut Pixel, stream: &mut TcpStream) -> std::io::Result<()> {
    let mut data = [0u8; 8];
    data[0] = b'G';
    data[1] = px.x as u8;
    data[2] = (px.x >> 8) as u8;
    data[3] = px.y as u8;
    data[4] = (px.y >> 8) as u8;
    stream.write_all(&data[..])?;
    let mut response = [0u8; 4];
    stream.read_exact(&mut response[..])?;
    px.color.0 = response[0];
    px.color.1 = response[1];
    px.color.2 = response[2];
    // ignore 4th byte for now
    Ok(())
}

fn encode_rect(rect: Rect, data: &mut [u8]) {
    // skip first byte
    data[1] = rect.x as u8;
    data[2] = (rect.x >> 8) as u8;
    data[3] = rect.y as u8;
    data[4] = (rect.y >> 8) as u8;
    data[5] = rect.w as u8;
    data[6] = rect.h as u8;
    data[7] = ((rect.w >> 8) & 0x0f) as u8 | ((rect.h >> 4) & 0xf0) as u8;
}

fn command_rectangle_print(colors: &[(u8, u8, u8)], rect: Rect, stream: &mut TcpStream) -> std::io::Result<()> {
    assert!(colors.len() == rect.w * rect.h);
    let mut data: Box<[u8; 1024]> = Box::new([0; 1024]);
    // first round: write actual command
    data[0] = b'p';
    encode_rect(rect, &mut data[0..8]);
    let mut data_fill_start: usize = 8;
    let mut pixel_idx = 0;
    while pixel_idx < colors.len() {
        // fill buffer
        while data_fill_start <= 1024 - 4 && pixel_idx < colors.len() {
            let col = colors[pixel_idx];
            data[data_fill_start] = col.0;
            data[data_fill_start + 1] = col.1;
            data[data_fill_start + 2] = col.2;
            data[data_fill_start + 3] = 0;
            pixel_idx += 1;
            data_fill_start += 4;
        }
        stream.write_all(&data[0..data_fill_start])?; // buffer may not be full in last round
        data_fill_start = 0; // reset buffer
    }
    Ok(())
}

fn command_rectangle_fill(color: (u8, u8, u8), rect: Rect, stream: &mut TcpStream) -> std::io::Result<()> {
    let mut data = [0u8; 12];
    // first round: write actual command
    data[0] = b'f';
    encode_rect(rect, &mut data[0..8]);
    data[8] = color.0;
    data[9] = color.1;
    data[10] = color.2;
    stream.write_all(&data[..])?;
    Ok(())
}

fn command_rectangle_get(colors: &mut [(u8, u8, u8)], rect: Rect, stream: &mut TcpStream) -> std::io::Result<()> {
    assert!(colors.len() == rect.w * rect.h);
    let mut command: [u8; 8] = [0; 8];
    command[0] = b'g';
    encode_rect(rect, &mut command[..]);
    stream.write_all(&command[..])?;
    // receive pixels
    let mut data: Box<[u8; 1024]> = Box::new([0; 1024]);
    let mut num_bytes_to_read: usize = rect.w * rect.h * 4;
    let mut pixel_idx = 0;
    while num_bytes_to_read > 0 {
        let mut read_size = num_bytes_to_read;
        if read_size > 1024 {
            read_size = 1024;
        }
        stream.read_exact(&mut data[0..read_size])?;
        num_bytes_to_read -= read_size;
        for i in (0..read_size).step_by(4) {
            colors[pixel_idx] = (data[i + 0], data[i + 1], data[i + 2]);
            pixel_idx += 1;
        }
    }
    Ok(())
}

fn main() -> std::io::Result<()> {
    let mut stream = TcpStream::connect("127.0.0.1:1337")?;

    // do random stuff
    let info = command_info(&mut stream)?;
    println!("{:?}", info);

    let rect = Rect { x: (info.width/2 - 5) as usize, y: (info.height/2 - 5) as usize, w: 10, h: 10 };
    let mut colors: Vec<(u8, u8, u8)> = Vec::new();
    for i in 0u8..100u8 {
        colors.push((i, i, i));
    }
    command_rectangle_print(&colors[..], rect, &mut stream)?;

    colors = vec![(0, 0, 0); (info.width * info.height) as usize];
    let rect = Rect { x: 0, y: 0, w: info.width as usize, h: info.height as usize };
    command_rectangle_get(&mut colors, rect, &mut stream)?;
    for color in &mut colors[..] {
        color.0 = 255u8 - color.0;
        color.1 = 255u8 - color.1;
        color.2 = 255u8 - color.2;
    }
    command_rectangle_print(&colors[..], rect, &mut stream)?;
    Ok(())
}
