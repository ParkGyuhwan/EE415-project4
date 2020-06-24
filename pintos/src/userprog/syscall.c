#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <devices/shutdown.h>
#include <threads/thread.h>
#include <filesys/filesys.h>
#include <filesys/file.h>
#include <userprog/process.h>
#include <devices/input.h>
#include "filesys/off_t.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
void check_address(void *addr);

struct file
  {
    struct inode *inode;
    off_t pos;
    bool deny_write;
  };

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  //printf ("system call!\n");
  //thread_exit ();
  void *esp = f->esp;
  int syscall_num;
  int arg[5];

  check_address(esp);
  syscall_num = *(int *)esp;

  switch(syscall_num){
	  case SYS_HALT:
		  halt();
		  break;
	  case SYS_EXIT:
		  get_argument(esp,arg,1);
		  exit(arg[0]);
		  break;
	  case SYS_EXEC:
		  get_argument(esp,arg,1);
		  check_address((void *)arg[0]);
		  f->eax = exec((const char *)arg[0]);
		  break;
	  case SYS_WAIT:
		  get_argument(esp,arg,1);
		  f->eax = wait(arg[0]);
		  break;
	  case SYS_CREATE:
		  get_argument(esp,arg,2);
		  check_address((void *)arg[0]);
		  f->eax = create((const char *)arg[0],(unsigned)arg[1]);
		  break;
	  case SYS_REMOVE:
		  get_argument(esp,arg,1);
		  check_address((void *)arg[0]);
		  f->eax=remove((const char *)arg[0]);
		  break;
	  case SYS_OPEN:
		  get_argument(esp,arg,1);
		  check_address((void *)arg[0]);
		  f->eax = open((const char *)arg[0]);
		  break;
	  case SYS_FILESIZE:
		  get_argument(esp,arg,1);
		  f->eax = filesize(arg[0]);
		  break;
	  case SYS_READ:
		  get_argument(esp,arg,3);
		  check_address((void *)arg[1]);
		  f->eax = read(arg[0],(void *)arg[1],(unsigned)arg[2]);
		  break;
	  case SYS_WRITE:
		  get_argument(esp,arg,3);
		  check_address((void *)arg[1]);
		  f->eax = write(arg[0],(void *)arg[1],(unsigned)arg[2]);
		  break;
	  case SYS_SEEK:
		  get_argument(esp,arg,2);
		  seek(arg[0],(unsigned)arg[1]);
		  break;
	  case SYS_TELL:
		  get_argument(esp,arg,1);
		  f->eax = tell(arg[0]);
		  break;
	  case SYS_CLOSE:
		  get_argument(esp,arg,1);
		  close(arg[0]);
		  break;
  }
}

void check_address(void *addr)
{
  if (!(is_user_vaddr (addr) && addr >= (void *)0x08048000UL))
    exit (-1);
  return;
}

void get_argument(void *esp, int *arg, int count)
{
  int i;
  void *stack_adr = esp + 4;
  if(count < 0)
    return;
  for(i = 0; i < count; i++){
    check_address(stack_adr);
    arg[i] = *(int *)stack_adr;
    stack_adr += 4;
  }
  return;
}

void halt(void)
{
  shutdown_power_off();
}

void exit(int status)
{
	struct thread *cur_process=thread_current();
	cur_process->process_exit_status = status;     
	printf("%s: exit(%d)\n",cur_process->name,status);
	thread_exit();
}

int wait (tid_t pid)
{
	int status;
	status = process_wait(pid);
	return status;
}

bool create(const char *file, unsigned initial_size)
{
	bool result;
	if(filesys_create(file,initial_size)==true)
		result=true;
	else
		result = false;
	return result;
}

bool remove(const char *file)
{
	bool result;
	if(filesys_remove(file)==true)
		result = true;
	else
		result = false;
	return result;
}

tid_t exec(const char *cmd_line)
{
	struct thread *child_process;
	tid_t pid;
	
	pid = process_execute(cmd_line);
	child_process = get_child_process(pid);	
	sema_down(&(child_process->load_semaphore));  

	if(child_process->load_success==true)
		return pid;
	else
		return -1;
}

int open(const char *file)
{
	int fd;
	struct file *new_file;
	
	new_file = filesys_open(file);

	if(new_file != NULL)
	{
		fd = process_add_file(new_file);
		if(strcmp(thread_current()->name, file) == 0)
			file_deny_write(new_file);
		return fd;
	}
	else
		return -1;
}

int filesize(int fd)
{
	int file_size;
	struct file *current_file;
	current_file = process_get_file(fd);
	if(current_file == NULL)
		return -1;
	else {
		file_size = file_length(current_file);
		return file_size;
	}
}

int read(int fd, void *buffer, unsigned size)
{
	int read_size = 0;
	struct file *current_file;
	char *read_buffer = (char *)buffer;
    
	lock_acquire(&file_lock);

	if(fd == 0) 
	{
		read_buffer[read_size]=input_getc();
		while(read_buffer[read_size] != '\n' && read_size < size)
		{
			read_size++;
			read_buffer[read_size]=input_getc();
		}
		read_buffer[read_size]='\0';
	}
	else
	{
		current_file = process_get_file(fd);
		if(current_file != NULL)
		{
			read_size = file_read(current_file,buffer,size);
		}
	}
	lock_release(&file_lock);
	return read_size;
}
/* write file */
int write(int fd, void *buffer, unsigned size)
{
	int write_size = -1;
	struct file *current_file;

	lock_acquire(&file_lock);
	if(fd == 1)
	{ 
		putbuf((const char *)buffer,size);
		write_size = size;
	}
	else
	{
		current_file = process_get_file(fd);
		if(current_file == NULL){
			lock_release(&file_lock);
			exit(-1);
		}
		else{
			if(current_file->deny_write){
				file_deny_write(current_file);
			}
			write_size = file_write(current_file,(const void *)buffer,size);
		}
	}
	lock_release(&file_lock);
	return write_size;
}

void seek(int fd, unsigned position)
{
	struct file *current_file;
	current_file = process_get_file(fd);
	
	if(current_file != NULL)
		file_seek(current_file,position);
}

unsigned tell(int fd)
{
	struct file *current_file;
	current_file = process_get_file(fd);
	unsigned offset = 0;

	if(current_file != NULL)
		offset = file_tell(current_file);
	return offset;
}

void  close(int fd)
{
	struct file *current_file;
	current_file = process_get_file(fd);
	if(current_file != NULL)
	{
		file_close(current_file);
		thread_current()->file_descriptor[fd]=NULL;
	}
}
